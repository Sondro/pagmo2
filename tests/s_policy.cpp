/* Copyright 2017-2018 PaGMO development team

This file is part of the PaGMO library.

The PaGMO library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 3 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The PaGMO library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the PaGMO library.  If not,
see https://www.gnu.org/licenses/. */

#if defined(_MSC_VER)

// Disable warnings from MSVC.
#pragma warning(disable : 4822)

#endif

#define BOOST_TEST_MODULE s_policy_test
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <initializer_list>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include <pagmo/s11n.hpp>
#include <pagmo/s_policies/select_best.hpp>
#include <pagmo/s_policy.hpp>
#include <pagmo/types.hpp>

using namespace pagmo;

BOOST_AUTO_TEST_CASE(type_traits_tests)
{
    BOOST_CHECK(!is_udsp<void>::value);
    BOOST_CHECK(!is_udsp<int>::value);
    BOOST_CHECK(!is_udsp<double>::value);

    struct udsp00 {
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const;
    };

    BOOST_CHECK(is_udsp<udsp00>::value);
    BOOST_CHECK(!is_udsp<const udsp00>::value);
    BOOST_CHECK(!is_udsp<const udsp00 &>::value);
    BOOST_CHECK(!is_udsp<udsp00 &>::value);

    struct no_udsp00 {
        void select(const individuals_group_t &, const vector_double::size_type &, const vector_double::size_type &,
                    const vector_double::size_type &, const vector_double::size_type &,
                    const vector_double::size_type &, const vector_double &) const;
    };

    BOOST_CHECK(!is_udsp<no_udsp00>::value);

    struct no_udsp01 {
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &);
    };

    BOOST_CHECK(!is_udsp<no_udsp01>::value);

    struct no_udsp02 {
        no_udsp02() = delete;
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const;
    };

    BOOST_CHECK(!is_udsp<no_udsp02>::value);
}

struct udsp1 {
    individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double &) const
    {
        return inds;
    }
    std::string foo = "hello world";
};

struct udsp2 {
    udsp2() = default;
    udsp2(const udsp2 &other) : foo{new std::string{*other.foo}} {}
    udsp2(udsp2 &&) = default;
    individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double &) const
    {
        return inds;
    }
    std::string get_name() const
    {
        return "frobniz";
    }
    std::unique_ptr<std::string> foo = std::unique_ptr<std::string>{new std::string{"hello world"}};
};

BOOST_AUTO_TEST_CASE(basic_tests)
{
    s_policy r;

    BOOST_CHECK(r.is<select_best>());
    BOOST_CHECK(!r.is<udsp1>());

    BOOST_CHECK(r.extract<select_best>() != nullptr);
    BOOST_CHECK(r.extract<udsp1>() == nullptr);

    BOOST_CHECK(static_cast<const s_policy &>(r).extract<select_best>() != nullptr);
    BOOST_CHECK(static_cast<const s_policy &>(r).extract<udsp1>() == nullptr);

    BOOST_CHECK(r.get_name() == "Select best");
    BOOST_CHECK(!r.get_extra_info().empty());

    BOOST_CHECK(s_policy(udsp1{}).get_extra_info().empty());
    BOOST_CHECK(!s_policy(udsp1{}).get_name().empty());

    // Constructors, assignments.
    // Generic constructor with copy.
    udsp1 r1;
    s_policy s_pol1{r1};
    BOOST_CHECK(r1.foo == "hello world");
    BOOST_CHECK(s_pol1.extract<udsp1>()->foo == "hello world");
    // Generic constructor with move.
    udsp2 r2;
    s_policy s_pol2{std::move(r2)};
    BOOST_CHECK(r2.foo.get() == nullptr);
    BOOST_CHECK(s_pol2.extract<udsp2>()->foo.get() != nullptr);
    BOOST_CHECK(*s_pol2.extract<udsp2>()->foo == "hello world");
    // Copy constructor.
    udsp2 r3;
    s_policy s_pol3{r3}, s_pol4{s_pol3};
    BOOST_CHECK(*s_pol4.extract<udsp2>()->foo == "hello world");
    BOOST_CHECK(s_pol4.extract<udsp2>()->foo.get() != s_pol3.extract<udsp2>()->foo.get());
    BOOST_CHECK(s_pol4.get_name() == "frobniz");
    // Move constructor.
    s_policy s_pol5{std::move(s_pol4)};
    BOOST_CHECK(*s_pol5.extract<udsp2>()->foo == "hello world");
    BOOST_CHECK(s_pol5.get_name() == "frobniz");
    // Revive s_pol4 via copy assignment.
    s_pol4 = s_pol5;
    BOOST_CHECK(*s_pol4.extract<udsp2>()->foo == "hello world");
    BOOST_CHECK(s_pol4.get_name() == "frobniz");
    // Revive s_pol4 via move assignment.
    s_policy s_pol6{std::move(s_pol4)};
    s_pol4 = std::move(s_pol5);
    BOOST_CHECK(*s_pol4.extract<udsp2>()->foo == "hello world");
    BOOST_CHECK(s_pol4.get_name() == "frobniz");
    // Self move-assignment.
    s_pol4 = std::move(*&s_pol4);
    BOOST_CHECK(*s_pol4.extract<udsp2>()->foo == "hello world");
    BOOST_CHECK(s_pol4.get_name() == "frobniz");

    // Minimal iostream test.
    {
        std::ostringstream oss;
        oss << r;
        BOOST_CHECK(!oss.str().empty());
    }

    // Minimal serialization test.
    {
        std::string before;
        std::stringstream ss;
        {
            before = boost::lexical_cast<std::string>(r);
            boost::archive::binary_oarchive oarchive(ss);
            oarchive << r;
        }
        r = s_policy{udsp1{}};
        BOOST_CHECK(r.is<udsp1>());
        BOOST_CHECK(before != boost::lexical_cast<std::string>(r));
        {
            boost::archive::binary_iarchive iarchive(ss);
            iarchive >> r;
        }
        BOOST_CHECK(before == boost::lexical_cast<std::string>(r));
        BOOST_CHECK(r.is<select_best>());
    }
}

BOOST_AUTO_TEST_CASE(optional_tests)
{
    // get_name().
    struct udsp_00 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
        std::string get_name() const
        {
            return "frobniz";
        }
    };
    BOOST_CHECK_EQUAL(s_policy{udsp_00{}}.get_name(), "frobniz");
    struct udsp_01 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
        // Missing const.
        std::string get_name()
        {
            return "frobniz";
        }
    };
    BOOST_CHECK(s_policy{udsp_01{}}.get_name() != "frobniz");

    // get_extra_info().
    struct udsp_02 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
        std::string get_extra_info() const
        {
            return "frobniz";
        }
    };
    BOOST_CHECK_EQUAL(s_policy{udsp_02{}}.get_extra_info(), "frobniz");
    struct udsp_03 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
        // Missing const.
        std::string get_extra_info()
        {
            return "frobniz";
        }
    };
    BOOST_CHECK(s_policy{udsp_03{}}.get_extra_info().empty());
}

BOOST_AUTO_TEST_CASE(stream_operator)
{
    struct udsp_00 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
    };
    {
        std::ostringstream oss;
        oss << s_policy{udsp_00{}};
        BOOST_CHECK(!oss.str().empty());
    }
    struct udsp_01 {
        individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return inds;
        }
        std::string get_extra_info() const
        {
            return "bartoppo";
        }
    };
    {
        std::ostringstream oss;
        oss << s_policy{udsp_01{}};
        const auto st = oss.str();
        BOOST_CHECK(boost::contains(st, "bartoppo"));
        BOOST_CHECK(boost::contains(st, "Extra info:"));
    }
}

BOOST_AUTO_TEST_CASE(selection)
{
    s_policy r0;

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {}, {}}, 0, 0, 0, 0, 0, {}), std::invalid_argument,
                          [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(),
                                  "an invalid group of individuals was passed to a selection policy of type 'Select "
                                  "best': the sets of individuals IDs, decision vectors and fitness vectors "
                                  "must all have the same sizes, but instead their sizes are 1, 0 and 0");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 0, 0, 0, 0, 0, {}), std::invalid_argument,
                          [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(),
                                  "a problem dimension of zero was passed to a selection policy of type 'Select best'");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 2, 0, 0, 0, {}), std::invalid_argument,
                          [](const std::invalid_argument &ia) {
                              return boost::contains(ia.what(),
                                                     "the integer dimension (2) passed to a selection policy of type "
                                                     "'Select best' is larger than the supplied problem dimension (1)");
                          });

    BOOST_CHECK_EXCEPTION(
        r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 0, 0, 0, 0, {}), std::invalid_argument,
        [](const std::invalid_argument &ia) {
            return boost::contains(
                ia.what(),
                "an invalid number of objectives (0) was passed to a selection policy of type 'Select best'");
        });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 0,
                                    std::numeric_limits<vector_double::size_type>::max(), 0, 0, {}),
                          std::invalid_argument, [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(), "the number of objectives ("
                                                 + std::to_string(std::numeric_limits<vector_double::size_type>::max())
                                                 + ") passed to a selection policy of type 'Select best' is too large");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 0, 1,
                                    std::numeric_limits<vector_double::size_type>::max(), 0, {}),
                          std::invalid_argument, [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(), "the number of equality constraints ("
                                                 + std::to_string(std::numeric_limits<vector_double::size_type>::max())
                                                 + ") passed to a selection policy of type 'Select best' is too large");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 0, 1, 0,
                                    std::numeric_limits<vector_double::size_type>::max(), {}),
                          std::invalid_argument, [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(), "the number of inequality constraints ("
                                                 + std::to_string(std::numeric_limits<vector_double::size_type>::max())
                                                 + ") passed to a selection policy of type 'Select best' is too large");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0}, {{1.}}, {{1.}}}, 1, 0, 1, 1, 1, {}), std::invalid_argument,
                          [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(),
                                  "the vector of tolerances passed to a selection policy of type 'Select best' has "
                                  "a dimension (0) which is inconsistent with the total number of constraints (2)");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0, 1}, {{1.}, {}}, {{1.}, {1.}}}, 1, 0, 1, 0, 0, {}),
                          std::invalid_argument, [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(), "not all the individuals passed to a selection policy of type 'Select "
                                             "best' have the expected dimension (1)");
                          });

    BOOST_CHECK_EXCEPTION(r0.select(individuals_group_t{{0, 1}, {{1.}, {1.}}, {{1.}, {}}}, 1, 0, 1, 0, 0, {}),
                          std::invalid_argument, [](const std::invalid_argument &ia) {
                              return boost::contains(
                                  ia.what(), "not all the individuals passed to a selection policy of type 'Select "
                                             "best' have the expected fitness dimension (1)");
                          });

    struct fail_0 {
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return individuals_group_t{{0}, {}, {}};
        }
        std::string get_name() const
        {
            return "fail_0";
        }
    };

    BOOST_CHECK_EXCEPTION(
        s_policy{fail_0{}}.select(individuals_group_t{{0, 1}, {{1.}, {1.}}, {{1.}, {1.}}}, 1, 0, 1, 0, 0, {}),
        std::invalid_argument, [](const std::invalid_argument &ia) {
            return boost::contains(ia.what(),
                                   "an invalid group of individuals was returned by a selection policy of type "
                                   "'fail_0': the sets of individuals IDs, decision vectors and fitness vectors "
                                   "must all have the same sizes, but instead their sizes are 1, 0 and 0");
        });

    struct fail_1 {
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return individuals_group_t{{0, 1}, {{1}, {}}, {{1}, {1}}};
        }
        std::string get_name() const
        {
            return "fail_1";
        }
    };

    BOOST_CHECK_EXCEPTION(
        s_policy{fail_1{}}.select(individuals_group_t{{0, 1}, {{1.}, {1.}}, {{1.}, {1.}}}, 1, 0, 1, 0, 0, {}),
        std::invalid_argument, [](const std::invalid_argument &ia) {
            return boost::contains(ia.what(), "not all the individuals returned by a selection "
                                              "policy of type 'fail_1' have the expected dimension (1)");
        });

    struct fail_2 {
        individuals_group_t select(const individuals_group_t &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double::size_type &, const vector_double::size_type &,
                                   const vector_double &) const
        {
            return individuals_group_t{{0, 1}, {{1}, {1}}, {{1}, {}}};
        }
        std::string get_name() const
        {
            return "fail_2";
        }
    };

    BOOST_CHECK_EXCEPTION(
        s_policy{fail_2{}}.select(individuals_group_t{{0, 1}, {{1.}, {1.}}, {{1.}, {1.}}}, 1, 0, 1, 0, 0, {}),
        std::invalid_argument, [](const std::invalid_argument &ia) {
            return boost::contains(ia.what(), "not all the individuals returned by a selection policy of type "
                                              "'fail_2' have the expected fitness dimension (1)");
        });
}

struct udsp_a {
    individuals_group_t select(const individuals_group_t &inds, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double::size_type &, const vector_double::size_type &,
                               const vector_double &) const
    {
        return inds;
    }
    std::string get_name() const
    {
        return "abba";
    }
    std::string get_extra_info() const
    {
        return "dabba";
    }
    template <typename Archive>
    void serialize(Archive &ar, unsigned)
    {
        ar &state;
    }
    int state = 42;
};

PAGMO_S11N_S_POLICY_EXPORT(udsp_a)

// Serialization tests.
BOOST_AUTO_TEST_CASE(s11n)
{
    s_policy s_pol0{udsp_a{}};
    BOOST_CHECK(s_pol0.extract<udsp_a>()->state == 42);
    s_pol0.extract<udsp_a>()->state = -42;
    // Store the string representation.
    std::stringstream ss;
    auto before = boost::lexical_cast<std::string>(s_pol0);
    // Now serialize, deserialize and compare the result.
    {
        boost::archive::binary_oarchive oarchive(ss);
        oarchive << s_pol0;
    }
    // Change the content of p before deserializing.
    s_pol0 = s_policy{};
    {
        boost::archive::binary_iarchive iarchive(ss);
        iarchive >> s_pol0;
    }
    auto after = boost::lexical_cast<std::string>(s_pol0);
    BOOST_CHECK_EQUAL(before, after);
    BOOST_CHECK(s_pol0.is<udsp_a>());
    BOOST_CHECK(s_pol0.extract<udsp_a>()->state = -42);
}

BOOST_AUTO_TEST_CASE(is_valid)
{
    s_policy p0;
    BOOST_CHECK(p0.is_valid());
    s_policy p1(std::move(p0));
    BOOST_CHECK(!p0.is_valid());
    p0 = s_policy{udsp_a{}};
    BOOST_CHECK(p0.is_valid());
    p1 = std::move(p0);
    BOOST_CHECK(!p0.is_valid());
    p0 = s_policy{udsp_a{}};
    BOOST_CHECK(p0.is_valid());
}

BOOST_AUTO_TEST_CASE(generic_assignment)
{
    s_policy p0;
    BOOST_CHECK(p0.is<select_best>());
    BOOST_CHECK(&(p0 = udsp_a{}) == &p0);
    BOOST_CHECK(p0.is_valid());
    BOOST_CHECK(p0.is<udsp_a>());
    p0 = udsp1{};
    BOOST_CHECK(p0.is<udsp1>());
    BOOST_CHECK((!std::is_assignable<s_policy, void>::value));
    BOOST_CHECK((!std::is_assignable<s_policy, int &>::value));
    BOOST_CHECK((!std::is_assignable<s_policy, const int &>::value));
    BOOST_CHECK((!std::is_assignable<s_policy, int &&>::value));
}
