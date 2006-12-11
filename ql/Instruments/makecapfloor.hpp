/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Ferdinando Ametrano

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file makecapfloor.hpp
    \brief Helper class to instantiate standard market cap/floor.
*/

#ifndef quantlib_instruments_makecapfloor_hpp
#define quantlib_instruments_makecapfloor_hpp

#include <ql/Instruments/capfloor.hpp>
#include <ql/Instruments/makevanillaswap.hpp>

namespace QuantLib {

    //! helper class
    /*! This class provides a more comfortable way
        to instantiate standard market cap and floor.
    */
    class MakeCapFloor {
      public:
        MakeCapFloor(CapFloor::Type capFloorType,
                     const Period& capFloorTenor, 
                     const boost::shared_ptr<IborIndex>& index,
                     Rate strike = Null<Rate>(),
                     const Period& forwardStart = 0*Days,
                     const boost::shared_ptr<PricingEngine>& engine =
                         boost::shared_ptr<PricingEngine>());

        operator CapFloor() const;
        operator boost::shared_ptr<CapFloor>() const ;

        MakeCapFloor& withNominal(Real n);
        MakeCapFloor& withEffectiveDate(const Date& effectiveDate,
                                        bool firstCapletExcluded);
        MakeCapFloor& withDiscountingTermStructure(
            const Handle<YieldTermStructure>& discountingTermStructure);

        MakeCapFloor& withTenor(const Period& t);
        MakeCapFloor& withCalendar(const Calendar& cal);
        MakeCapFloor& withConvention(BusinessDayConvention bdc);
        MakeCapFloor& withTerminationDateConvention(BusinessDayConvention bdc);
        MakeCapFloor& withForward(bool flag = true);
        MakeCapFloor& withEndOfMonth(bool flag = true);
        MakeCapFloor& withFirstDate(const Date& d);
        MakeCapFloor& withNextToLastDate(const Date& d);
        MakeCapFloor& withDayCount(const DayCounter& dc);
        
      private:
        CapFloor::Type capFloorType_;
        Rate strike_;
        bool firstCapletExcluded_;

        boost::shared_ptr<PricingEngine> engine_;
        MakeVanillaSwap makeVanillaSwap_;
    };

}

#endif
