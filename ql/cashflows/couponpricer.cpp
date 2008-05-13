/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2007 Cristina Duminuco

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/cashflows/digitalcmscoupon.hpp>
#include <ql/cashflows/digitaliborcoupon.hpp>
#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantLib {

//===========================================================================//
//                              BlackIborCouponPricer                        //
//===========================================================================//

    void BlackIborCouponPricer::initialize(const FloatingRateCoupon& coupon) {
        coupon_ = dynamic_cast<const IborCoupon*>(&coupon);
        gearing_ = coupon_->gearing();
        spread_ = coupon_->spread();
        Date paymentDate = coupon_->date();
        const boost::shared_ptr<InterestRateIndex>& index = coupon_->index();
        Handle<YieldTermStructure> rateCurve = index->termStructure();

        Date today = Settings::instance().evaluationDate();

        if (paymentDate > today)
            discount_ = rateCurve->discount(paymentDate);
        else
            discount_ = 1.0;

        spreadLegValue_ = spread_ * coupon_->accrualPeriod()* discount_;
    }

    Real BlackIborCouponPricer::swapletPrice() const {
        // past or future fixing is managed in InterestRateIndex::fixing()

        Real swapletPrice =
           adjustedFixing()* coupon_->accrualPeriod() * discount_;
        return gearing_ * swapletPrice + spreadLegValue_;
    }

    Rate BlackIborCouponPricer::swapletRate() const {
        return swapletPrice()/(coupon_->accrualPeriod()*discount_);
    }

    Real BlackIborCouponPricer::capletPrice(Rate effectiveCap) const {
        Real capletPrice = optionletPrice(Option::Call, effectiveCap);
        return gearing_ * capletPrice;
    }

    Rate BlackIborCouponPricer::capletRate(Rate effectiveCap) const {
        return capletPrice(effectiveCap)/(coupon_->accrualPeriod()*discount_);
    }

    Real BlackIborCouponPricer::floorletPrice(Rate effectiveFloor) const {
        Real floorletPrice = optionletPrice(Option::Put, effectiveFloor);
        return gearing_ * floorletPrice;
    }

    Rate BlackIborCouponPricer::floorletRate(Rate effectiveFloor) const {
        return floorletPrice(effectiveFloor)/
            (coupon_->accrualPeriod()*discount_);
    }

    Real BlackIborCouponPricer::optionletPrice(Option::Type optionType,
                                               Real effStrike) const {
        Date fixingDate = coupon_->fixingDate();
        if (fixingDate <= Settings::instance().evaluationDate()) {
            // the amount is determined
            Real a, b;
            if (optionType==Option::Call) {
                a = coupon_->indexFixing();
                b = effStrike;
            } else {
                a = effStrike;
                b = coupon_->indexFixing();
            }
            return std::max(a - b, 0.0)* coupon_->accrualPeriod()*discount_;
        } else {
            QL_REQUIRE(!capletVolatility().empty(),
                       "missing optionlet volatility");
            // not yet determined, use Black model
            Real variance =
                std::sqrt(capletVolatility()->blackVariance(fixingDate,
                                                            effStrike));
            Rate fixing = blackFormula(optionType,
                                       effStrike,
                                       adjustedFixing(),
                                       variance);
            return fixing * coupon_->accrualPeriod() * discount_;
        }
    }

    Rate BlackIborCouponPricer::adjustedFixing(Rate fixing) const {

        Real adjustement = 0.0;

        if (fixing == Null<Rate>())
            fixing = coupon_->indexFixing();

        if (!coupon_->isInArrears()) {
            adjustement = 0.0;
        } else {
            // see Hull, 4th ed., page 550
            QL_REQUIRE(!capletVolatility().empty(),
                       "missing optionlet volatility");
            Date d1 = coupon_->fixingDate(),
                 referenceDate = capletVolatility()->referenceDate();
            if (d1 <= referenceDate) {
                adjustement = 0.0;
            } else {
                Date d2 = coupon_->index()->maturityDate(d1);
                Time tau = coupon_->index()->dayCounter().yearFraction(d1, d2);
                Real variance = capletVolatility()->blackVariance(d1, fixing);
                adjustement = fixing*fixing*variance*tau/(1.0+fixing*tau);
            }
        }
        return fixing + adjustement;
    }

//===========================================================================//
//                         CouponSelectorToSetPricer                         //
//===========================================================================//

    namespace {

        class PricerSetter : public AcyclicVisitor,
                             public Visitor<CashFlow>,
                             public Visitor<Coupon>,
                             public Visitor<IborCoupon>,
                             public Visitor<CmsCoupon>,
                             public Visitor<CappedFlooredIborCoupon>,
                             public Visitor<CappedFlooredCmsCoupon>,
                             public Visitor<DigitalIborCoupon>,
                             public Visitor<DigitalCmsCoupon>,
                             public Visitor<RangeAccrualFloatersCoupon>{
          private:
            const boost::shared_ptr<FloatingRateCouponPricer> pricer_;
          public:
            PricerSetter(
                    const boost::shared_ptr<FloatingRateCouponPricer>& pricer)
            : pricer_(pricer) {}

            void visit(CashFlow& c);
            void visit(Coupon& c);
            void visit(IborCoupon& c);
            void visit(CappedFlooredIborCoupon& c);
            void visit(DigitalIborCoupon& c);
            void visit(CmsCoupon& c);
            void visit(CappedFlooredCmsCoupon& c);
            void visit(DigitalCmsCoupon& c);
            void visit(RangeAccrualFloatersCoupon& c);
        };

        void PricerSetter::visit(CashFlow&) {
            // nothing to do
        }

        void PricerSetter::visit(Coupon&) {
            // nothing to do
        }

        void PricerSetter::visit(IborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        void PricerSetter::visit(DigitalIborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        void PricerSetter::visit(CappedFlooredIborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        void PricerSetter::visit(CmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        void PricerSetter::visit(CappedFlooredCmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        void PricerSetter::visit(DigitalCmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        void PricerSetter::visit(RangeAccrualFloatersCoupon& c) {
            const boost::shared_ptr<RangeAccrualPricer> rangeAccrualPricer =
                boost::dynamic_pointer_cast<RangeAccrualPricer>(pricer_);
            QL_REQUIRE(rangeAccrualPricer,
                       "pricer not compatible with range-accrual coupon");
            c.setPricer(rangeAccrualPricer);
        }

    }

    void setCouponPricer(
                  const Leg& leg,
                  const boost::shared_ptr<FloatingRateCouponPricer>& pricer) {
        PricerSetter setter(pricer);
        for (Size i=0; i<leg.size(); ++i) {
            leg[i]->accept(setter);
        }
    }

    void setCouponPricers(
            const Leg& leg,
            const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >&
                                                                    pricers) {
        Size nCashFlows = leg.size();
        QL_REQUIRE(nCashFlows>0, "no cashflows");

        Size nPricers = pricers.size();
        QL_REQUIRE(nCashFlows >= nPricers,
                   "mismatch between leg size (" << nCashFlows <<
                   ") and number of pricers (" << nPricers << ")");

        for (Size i=0; i<nCashFlows; ++i) {
            PricerSetter setter(i<nPricers ? pricers[i] : pricers[nPricers-1]);
            leg[i]->accept(setter);
        }
    }

}
