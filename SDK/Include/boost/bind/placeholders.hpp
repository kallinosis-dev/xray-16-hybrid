#ifndef BOOST_BIND_PLACEHOLDERS_HPP_INCLUDED
#define BOOST_BIND_PLACEHOLDERS_HPP_INCLUDED

#if _MSC_VER >= 1020
#pragma once
#endif

//
//  bind/placeholders.hpp - _N definitions
//
//  Copyright (c) 2002 Peter Dimov and Multi Media Ltd.
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//
//  See http://www.boost.org/libs/bind/bind.html for documentation.
//

#include <boost/bind/arg.hpp>
#include <boost/config.hpp>

namespace
{

#if 0 // disable in C++17
#if defined(__BORLANDC__)

static inline boost::arg<1> _1() { return boost::arg<1>(); }
static inline boost::arg<2> _2() { return boost::arg<2>(); }
static inline boost::arg<3> _3() { return boost::arg<3>(); }
static inline boost::arg<4> _4() { return boost::arg<4>(); }
static inline boost::arg<5> _5() { return boost::arg<5>(); }
static inline boost::arg<6> _6() { return boost::arg<6>(); }
static inline boost::arg<7> _7() { return boost::arg<7>(); }
static inline boost::arg<8> _8() { return boost::arg<8>(); }
static inline boost::arg<9> _9() { return boost::arg<9>(); }

#elif (defined(BOOST_MSVC) && BOOST_MSVC <= 1300) || (defined(__DECCXX_VER) && __DECCXX_VER <= 60590031) || defined(__MWERKS__)

static boost::arg<1> _1;
static boost::arg<2> _2;
static boost::arg<3> _3;
static boost::arg<4> _4;
static boost::arg<5> _5;
static boost::arg<6> _6;
static boost::arg<7> _7;
static boost::arg<8> _8;
static boost::arg<9> _9;

#else

boost::arg<1> _1;
boost::arg<2> _2;
boost::arg<3> _3;
boost::arg<4> _4;
boost::arg<5> _5;
boost::arg<6> _6;
boost::arg<7> _7;
boost::arg<8> _8;
boost::arg<9> _9;

#endif
#endif

} // unnamed namespace

#endif // #ifndef BOOST_BIND_PLACEHOLDERS_HPP_INCLUDED
