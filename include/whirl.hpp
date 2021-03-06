#ifndef __WHIRL_HPP__
#define __WHIRL_HPP__


#include <istream>
#include <vector>
#include <optional>
#include <algorithm>
#include <tuple>

#include "type_traits.hpp"
#include "tokens.hpp"


namespace whirl
{

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // parsing error handling
    ////////////////////////////////////////////////////////////////////////////////////////////////

    struct code_position
    {
        unsigned row;
        unsigned col;

        template <typename C, typename = requires_t<is_character_type<C>>>
        constexpr void update(C chr) noexcept
        {
            if (chr == '\n')
            {
                this->row++;
                this->col = 0;
            }
            else
            {
                this->col++;
            }
        }
    };

    struct unexpected_input { };


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // basic bound predicates
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename C>
    struct bound_is_predicate
    {

        static_assert(is_character_type_v<C>);


        explicit constexpr bound_is_predicate(const C& cmp)
            : cmp{ cmp }
        { }

        template <typename I, typename = requires_t<is_compatible_input_source_type<I, C>>>
        constexpr bool is(I& ins) const
        {
            return input_source_traits<I>::look_ahead(ins) == this->cmp;
        }

        C cmp;

    };

    template <typename C>
    struct bound_is_not_predicate
    {

        static_assert(is_character_type_v<C>);


        explicit constexpr bound_is_not_predicate(const C& cmp)
            : cmp{ cmp }
        { }

        template <typename I, typename = requires_t<is_compatible_input_source_type<I, C>>>
        constexpr bool is(I& ins) const
        {
            return input_source_traits<I>::look_ahead(ins) != this->cmp;
        }

        C cmp;

    };

    template <typename... Cs>
    struct bound_is_one_of_predicate
    {

        static_assert(are_character_types_v<Cs...>);


        explicit constexpr bound_is_one_of_predicate(const Cs&... cmps)
            : cmps{ cmps... }
        { }

        template <typename I, typename = requires_t<is_compatible_input_source_type<I, Cs>...>>
        constexpr bool is(I& ins) const
        {
            return std::apply(
                [&ins](const auto&... cmps) {
                    return ((input_source_traits<I>::look_ahead(ins) == cmps) || ...);
                },
                this->cmps
            );
        }

        std::tuple<Cs...> cmps;

    };

    template <typename... Cs>
    struct bound_is_none_of_predicate
    {

        static_assert(are_character_types_v<Cs...>);


        explicit constexpr bound_is_none_of_predicate(const Cs&... cmps)
            : cmps{ cmps... }
        { }

        template <typename I, typename = requires_t<is_compatible_input_source_type<I, Cs>...>>
        constexpr bool is(I& ins) const
        {
            return std::apply(
                [&ins](const auto&... cmps) {
                    return ((input_source_traits<I>::look_ahead(ins) != cmps) && ...);
                },
                this->cmps
            );
        }

        std::tuple<Cs...> cmps;

    };

    struct bound_is_end_predicate
    {

        template <typename I, typename = requires_t<is_input_source_type<I>>>
        constexpr bool is(I& ins) const
        {
            return input_source_traits<I>::is_end(ins);
        }

    };

    struct bound_is_character_predicate
    {

        template <typename I, typename = requires_t<is_input_source_type<I>>>
        constexpr bool is(I& ins) const
        {
            return !input_source_traits<I>::is_end(ins);
        }

    };


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // composed bound predicates
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename P1, typename P2>
    struct bound_predicate_conjunction
    {

        static_assert(is_bound_predicate_v<P1>);
        static_assert(is_bound_predicate_v<P2>);


        explicit constexpr bound_predicate_conjunction(P1 p1, P2 p2)
            : p1{ p1 }
            , p2{ p2 }
        { }

        template <typename T>
        constexpr bool is(T& ins) const
        {
            return this->p1(ins) && this->p2(ins);
        }

        P1 p1;
        P2 p2;

    };

    template <typename P1, typename P2>
    struct bound_predicate_disjunction
    {

        static_assert(is_bound_predicate_v<P1>);
        static_assert(is_bound_predicate_v<P2>);


        explicit constexpr bound_predicate_disjunction(const P1& pred1, const P2& pred2)
            : pred1{ pred1 }
            , pred2{ pred2 }
        { }

        template <typename I>
        constexpr bool is(I& ins) const
        {
            return this->pred1(ins) || this->pred2(ins);
        }

        P1 pred1;
        P2 pred2;

    };

    template <typename P>
    struct bound_predicate_negation
    {

        static_assert(is_bound_predicate_v<P>);


        explicit constexpr bound_predicate_negation(const P& pred)
            : pred{ pred }
        { }

        template <typename I>
        constexpr bool is(I& ins) const
        {
            return !this->pred(ins);
        }

        P pred;

    };


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // universal logical bound predicate operations
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename P, typename = requires_t<is_bound_predicate<P>>>
    constexpr auto operator!(const P& p)
    {
        return bound_predicate_negation(p);
    }

    template <
        typename P1,
        typename P2,
        typename = requires_t<is_bound_predicate<P1>>,
        typename = requires_t<is_bound_predicate<P2>>
    >
    constexpr auto operator&&(const P1& p1, const P2& p2)
    {
        return bound_predicate_conjunction{ p1, p2 };
    }

    template <
        typename P1,
        typename P2,
        typename = requires_t<is_bound_predicate<P1>>,
        typename = requires_t<is_bound_predicate<P2>>
    >
    constexpr auto operator||(const P1& p1, const P2& p2)
    {
        return bound_predicate_disjunction{ p1, p2 };
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // specialzed optimized logical bound predicate operations
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename C, typename = requires_t<is_character_type<C>>>
    constexpr auto operator!(const bound_is_predicate<C>& p)
    {
        return bound_is_not_predicate<C>{ p.cmp };
    }

    template <typename C, typename = requires_t<is_character_type<C>>>
    constexpr auto operator!(const bound_is_not_predicate<C>& p)
    {
        return bound_is_predicate<C>{ p.cmp };
    }

    template <typename... Cs, typename = requires_t<are_character_types<Cs...>>>
    constexpr auto operator!(const bound_is_one_of_predicate<Cs...>& p)
    {
        return std::apply(
            [](const auto&... cmps) {
                return bound_is_none_of_predicate<Cs...>{ cmps... };
            },
            p.cmps
        );
    }

    template <typename... Cs, typename = requires_t<are_character_types<Cs...>>>
    constexpr auto operator!(const bound_is_none_of_predicate<Cs...>& p)
    {
        return std::apply(
            [](const auto&... cmps) {
                return bound_is_one_of_predicate<Cs...>{ cmps... };
            },
            p.cmps
        );
    }

    template <typename C1, typename C2, typename = requires_t<are_character_types<C1, C2>>>
    constexpr auto operator||(const bound_is_predicate<C1>& lhs, const bound_is_predicate<C2>& rhs)
    {
        return bound_is_one_of_predicate{ lhs.cmp, rhs.cmp };
    }

    template <typename... Cs, typename C, typename = requires_t<are_character_types<C, Cs...>>>
    constexpr auto operator||(
        const bound_is_one_of_predicate<Cs...>& lhs, const bound_is_predicate<C>& rhs
    )
    {
        return std::apply(
            [&rhs](const auto&... cmps) {
                return bound_is_one_of_predicate{ cmps..., rhs.cmp };
            },
            lhs.cmps
        );
    }

    template <typename C, typename... Cs, typename = requires_t<are_character_types<C, Cs...>>>
    constexpr auto operator||(
        const bound_is_predicate<C>& lhs, const bound_is_one_of_predicate<Cs...>& rhs
    )
    {
        return rhs || lhs;
    }

    template <
        typename... Cs1, typename... Cs2, typename = requires_t<are_character_types<Cs1..., Cs2...>>
    >
    constexpr auto operator||(
        const bound_is_one_of_predicate<Cs1...>& lhs, const bound_is_one_of_predicate<Cs2...>& rhs
    )
    {
        return std::apply(
            [&rhs](const auto&... cmps) {
                return (rhs || ... || cmps);
            },
            lhs.cmps
        );
    }

    template <typename C1, typename C2, typename = requires_t<are_character_types<C1, C2>>>
    constexpr auto operator&&(
        const bound_is_not_predicate<C1>& lhs, const bound_is_not_predicate<C2>& rhs
    )
    {
        return bound_is_none_of_predicate{ lhs.cmp, rhs.cmp };
    }

    template <
        typename... Cs,
        typename C,
        typename = requires_t<is_character_type<Cs>...>,
        typename = requires_t<is_character_type<C>>
    >
    constexpr auto operator&&(
        const bound_is_none_of_predicate<Cs...>& lhs, const bound_is_not_predicate<C>& rhs
    )
    {
        return std::apply(
            [&rhs](const auto&... cmps) {
                return bound_is_one_of_predicate<Cs..., C>{ cmps..., rhs.cmp };
            },
            lhs.cmps
        );
    }

    template <typename C, typename... Cs, typename = requires_t<are_character_types<C, Cs...>>>
    constexpr auto operator&&(
        const bound_is_not_predicate<C>& lhs, const bound_is_none_of_predicate<Cs...>& rhs
    )
    {
        return rhs || lhs;
    }

    template <
        typename... Cs1, typename... Cs2, typename = requires_t<are_character_types<Cs1..., Cs2...>>
    >
    constexpr auto operator&&(
        const bound_is_none_of_predicate<Cs1...>& lhs,
        const bound_is_none_of_predicate<Cs2...>& rhs
    )
    {
        return std::apply(
            [&rhs](const auto&... cmps) {
                return (rhs || ... || cmps);
            },
            lhs.cmps
        );
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // bound predicate factories
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <
        typename C,
        typename = requires_t<is_character_type<C>>
    >
    constexpr auto is(const C& cmp)
    {
        return bound_is_predicate{ cmp };
    }

    template <
        typename P,
        typename = requires_t<is_bound_predicate<P>>,
        typename = void
    >
    constexpr auto is(const P& pred)
    {
        return pred;
    }

    template <
        typename I,
        typename C,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_character_type<C>>
    >
    constexpr auto is(I& ins, const C& cmp)
    {
        return input_source_traits<I>::look_ahead(ins) != cmp;
    }

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = void
    >
    constexpr auto is(I& ins, const P& pred)
    {
        return pred.is(ins);
    }

    template <typename C, typename = requires_t<is_character_type<C>>>
    constexpr auto is_not(const C& cmp)
    {
        return bound_is_not_predicate{ cmp };
    }

    template <typename... Cs, typename = requires_t<are_character_types<Cs...>>>
    constexpr auto is_one_of(const Cs&... cmp)
    {
        return bound_is_one_of_predicate{ cmp... };
    }

    template <typename... Cs, typename = requires_t<are_character_types<Cs...>>>
    constexpr auto is_none_of(const Cs&... cmp)
    {
        return bound_is_none_of_predicate{ cmp... };
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // predefined bound predicates
    ////////////////////////////////////////////////////////////////////////////////////////////////

    constexpr auto end            = bound_is_end_predicate{};
    constexpr auto character      = bound_is_character_predicate{};
    constexpr auto blank          = is_one_of(' ', '\t');
    constexpr auto space          = is_one_of(' ', '\t', '\n');
    constexpr auto zero           = is('0');
    constexpr auto non_zero_digit = is_one_of('1', '2', '3', '4', '5', '6', '7', '8', '9');
    constexpr auto digit          = is(zero) || is(non_zero_digit);
    constexpr auto positive_sign  = is('+');
    constexpr auto negative_sign  = is('-');
    constexpr auto sign           = is(positive_sign) || is(negative_sign);


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // transformators
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename R>
    struct as_transform
    {
        constexpr explicit as_transform(const R& res)
            noexcept(std::is_nothrow_copy_constructible_v<R>)
            : res{ res }
        { }

        constexpr explicit as_transform(R&& res)
            noexcept(std::is_nothrow_move_constructible_v<R>)
            : res{ std::move(res) }
        { }

        template <typename C, typename = requires_t<is_character_type<C>>>
        constexpr auto operator()(const C&) const
        {
            return res;
        }

        R res;
    };

    struct as_is_transform
    {
        template <typename C, typename = requires_t<is_character_type<C>>>
        constexpr auto operator()(const C& chr) const
        {
            return chr;
        }
    };

    template <typename N, typename = std::enable_if_t<std::is_arithmetic_v<N>>>
    struct as_digit_transform
    {
        template <typename C, typename = requires_t<is_character_type<C>>>
        constexpr auto operator()(const C& chr) const
        {
            return DigitSequence{ chr - '0' };
        }
    };


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // transformator factories
    ////////////////////////////////////////////////////////////////////////////////////////////////

    constexpr auto as_is = as_is_transform{};

    template <typename N>
    constexpr auto as_digit = as_digit_transform<N>{};

    template <typename R>
    constexpr auto as(R&& res)
    {
        return as_transform{ std::forward<R>(res) };
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // 'next' overloads
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename I, typename = requires_t<is_input_source_type<I>>>
    constexpr void next(I& ins)
    {
        if (input_source_traits<I>::is_end(ins))
            throw unexpected_input{};

        input_source_traits<I>::ignore(ins);
    }

    template <
        typename I,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next(I& ins, T trans)
    {
        if (input_source_traits<I>::is_end(ins))
            throw unexpected_input{};

        return trans(input_source_traits<I>::read(ins));
    }

    template <
        typename V,
        typename I,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next(V init, I& ins, T trans)
    {
        if (input_source_traits<I>::is_end(ins))
            throw unexpected_input{};

        return concat(init, trans(input_source_traits<I>::read(ins)));
    }


    template <typename I, typename = requires_t<is_input_source_type<I>>>
    constexpr void next(I& ins, code_position& pos)
    {
        pos.update(next(ins, as_is));
    }

    template <
        typename I,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next(I& ins, code_position& pos, const T& trans)
    {
        const auto chr = next(ins, as_is);

        pos.update(chr);

        return trans(chr);
    }

    template <
        typename V,
        typename I,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next(V init, I& ins, code_position& pos, const T& trans)
    {
        const auto chr = next(ins, as_is);

        pos.update(chr);

        return concat(init, trans(chr));
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // 'next_is' overloads
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_is(I& ins, const P& pred)
    {
        if(!pred(ins))
            throw unexpected_input{};

        next(ins);
    }

    template <
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_is(I& ins, const P& pred, const T& trans)
    {
        if(!pred(ins))
            throw unexpected_input{};

        return next(ins, trans);
    }

    template <
        typename V,
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_is(V init, I& ins, const P& pred, const T& trans)
    {
        if(!pred(ins))
            throw unexpected_input{};

        return concat(init, next(ins, trans));
    }

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_is(I& ins, code_position& pos, const P& pred)
    {
        if(!pred.is(ins))
            throw unexpected_input{};

        if constexpr(std::is_same_v<P, bound_is_end_predicate>)
            return;

        next(ins, pos);
    }

    template <
        typename P,
        typename I,
        typename T,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_is(I& ins, code_position& pos, const P& pred, const T& trans)
    {
        if(!pred(ins))
            throw unexpected_input{};

        if constexpr(std::is_same_v<P, bound_is_end_predicate>)
            return;

        return next(ins, pos, trans);
    }

    template <
        typename V,
        typename P,
        typename I,
        typename T,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_is(V init, I& ins, code_position& pos, const P& pred, const T& trans)
    {
        if(!pred(ins))
            throw unexpected_input{};

        if constexpr(std::is_same_v<P, bound_is_end_predicate>)
            return;

        return concat(init, next(ins, pos, trans));
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // 'next_if' overloads
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_if(I& ins, const P& pred)
    {
        if (pred(ins))
            next(ins);
    }

    template <
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_if(I& ins, const P& pred, const T& trans)
        -> std::optional<decltype(next(ins, trans))>
    {
        if (pred(ins))
            return next(ins, trans);
        else
            return std::nullopt;
    }

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_if(I& ins, code_position& pos, const P& pred)
    {
        if (pred(ins))
           next(ins, pos);
    }

    template <
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr std::optional<typename I::char_type> next_if(
        I& ins, code_position& pos, const P& pred, const T& trans)
    {
        if (pred(ins))
            return next(ins, pos, trans);
        else
            return std::nullopt;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // 'next_while' overloads
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_while(I& ins, const P& pred)
    {
        while (pred(ins))
            next(ins);
    }

    template <
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_while(I& ins, const P& pred, const T& trans)
    {
        decltype(concat(next(ins, trans), next(ins, trans))) result;

        while (pred(ins))
            result = concat(result, next(ins, trans));

        return result;
    }

    template <
        typename V,
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_while(V init, I& ins, const P& pred, const T& trans)
    {
        while (pred(ins))
            init = concat(init, next(ins, trans));

        return init;
    }

    template <
        typename I,
        typename P,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>
    >
    constexpr void next_while(I& ins, code_position& pos, const P& pred)
    {
        while (pred.is(ins))
            next(ins, pos);
    }

    template <
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_while(I& ins, code_position& pos, const P& pred, const T& trans)
    {
        decltype(concat(next(ins, trans), next(ins, trans))) result;

        while (pred.is(ins))
            result = concat(result, next(ins, pos, trans));

        return result;
    }

    template <
        typename V,
        typename I,
        typename P,
        typename T,
        typename = requires_t<is_input_source_type<I>>,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_while(V init, I& ins, code_position& pos, P pred, T trans)
    {
        while (pred.is(ins))
            init = concat(std::move(init), next(ins, pos, std::move(trans)));

        return init;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // bound consumers
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T>
    struct bound_transforming_read
    {

        static_assert(is_transformator_v<T>);


        explicit constexpr bound_transforming_read(const T& trans)
            : trans{ trans }
        { }

        template <typename I>
        constexpr auto operator()(I& ins) const
        {
            return next(ins, this->trans);
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins) const
        {
            return next(init, ins, this->trans);
        }

        template <typename I>
        constexpr auto operator()(I& ins, code_position& pos) const
        {
            return next(ins, pos, this->trans);
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins, code_position& pos) const
        {
            return next(init, ins, pos, this->trans);
        }

        T trans;

    };

    template <typename P>
    struct bound_conditional_read
    {

        static_assert(is_bound_predicate_v<P>);


        explicit constexpr bound_conditional_read(const P& pred)
            : pred{ pred }
        { }

        template <typename I>
        constexpr void operator()(I& ins) const
        {
            next_is(ins, this->pred);
        }

        template <typename I>
        constexpr void operator()(I& ins, code_position& pos) const
        {
            next_is(ins, this->pred, pos);
        }

        P pred;

    };

    template <typename P, typename T, typename A>
    struct bound_ord_conditional_transforming_read
    {

        static_assert(is_bound_predicate_v<P>);
        static_assert(is_transformator_v<T>);


        constexpr bound_ord_conditional_transforming_read(
            const P& pred, const T& trans, const A& alt)
            : pred{ pred }
            , trans{ trans }
            , alt{ alt }
        { }

        template <typename I>
        constexpr auto operator()(I& ins) const
        {
            if(is(ins, this->pred))
                return next(ins, this->trans);
            else
                return this->alt;
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins) const
        {
            if(is(ins, this->pred))
                return concat(init, next(ins, this->trans));
            else
                return concat(init, this->alt);
        }

        template <typename I>
        constexpr auto operator()(I& ins, code_position& pos) const
        {
            if(is(ins, this->pred))
                return next(ins, pos, this->trans);
            else
                return this->alt;
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins, code_position& pos) const
        {
            if(is(ins, this->pred))
                return concat(init, next(ins, pos, this->trans));
            else
                return concat(init, this->alt);
        }

        P pred;
        T trans;
        A alt;

    };

    template <typename P, typename T>
    struct bound_transforming_conditional_read
    {

        static_assert(is_bound_predicate_v<P>);
        static_assert(is_transformator_v<T>);


        explicit constexpr bound_transforming_conditional_read(const P& pred, const T& trans)
            : pred{ pred }
            , trans{ trans }
        { }

        template <typename I>
        constexpr auto operator()(I& ins) const
        {
            return next_is(ins, this->pred, this->trans);
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins) const
        {
            return next_is(init, ins, this->pred, this->trans);
        }

        template <typename I>
        constexpr auto operator()(I& ins, code_position& pos) const
        {
            return next_is(ins, pos, this->pred, this->trans);
        }

        template <typename V, typename I>
        constexpr auto operator()(V init, I& ins, code_position& pos) const
        {
            return next_is(init, ins, pos, this->pred, this->trans);
        }

        template <typename A>
        constexpr auto operator||(const A& alt)
        {
            return bound_ord_conditional_transforming_read{ this->pred, this->trans, alt };
        }

        P pred;
        T trans;

    };

    template <typename P>
    struct bound_conditional_multi_read
    {

        static_assert(is_bound_predicate_v<P>);


        explicit constexpr bound_conditional_multi_read(const P& pred)
            : pred{ pred }
        { }

        template <typename I>
        constexpr void operator()(I& ins) const
        {
            next_while(ins, this->pred);
        }

        template <typename I>
        constexpr void operator()(I& ins, code_position& pos) const
        {
            next_while(ins, pos, this->pred);
        }

        P pred;

    };

    template <typename P, typename T>
    struct bound_transforming_conditional_multi_read
    {

        static_assert(is_bound_predicate_v<P>);
        static_assert(is_transformator_v<T>);

        explicit constexpr bound_transforming_conditional_multi_read(const P& pred, const T& trans)
            : pred{ pred }
            , trans{ trans }
        { }

        template <typename I>
        auto operator()(I& ins) const
        {
            return next_while(ins, this->pred, this->trans);
        }

        template <typename V, typename I>
        auto operator()(V init, I& ins) const
        {
            return next_while(init, ins, this->pred, this->trans);
        }

        template <typename I>
        auto operator()(I& ins, code_position& pos) const
        {
            return next_while(ins, pos, this->pred, this->trans);
        }

        template <typename V, typename I>
        auto operator()(V init, I& ins, code_position& pos) const
        {
            return next_while(init, ins, pos, this->pred, this->trans);
        }

        P pred;
        T trans;

    };


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // bound consumer factories
    ////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename = requires_t<is_transformator<T>>>
    constexpr auto next(const T& trans)
    {
        return bound_transforming_read{ trans };
    }

    template <typename P, typename = requires_t<is_bound_predicate<P>>>
    constexpr auto next_is(const P& pred)
    {
        return bound_conditional_read{ pred };
    }

    template <
        typename P,
        typename T,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_is(const P& pred, const T& trans)
    {
        return bound_transforming_conditional_read{ pred, trans };
    }

    template <typename P, typename = requires_t<is_bound_predicate<P>>>
    constexpr auto next_while(const P& pred)
    {
        return bound_conditional_multi_read{ pred };
    }

    template <
        typename P,
        typename T,
        typename = requires_t<is_bound_predicate<P>>,
        typename = requires_t<is_transformator<T>>
    >
    constexpr auto next_while(const P& pred, const T& trans)
    {
        return bound_transforming_conditional_multi_read{ pred, trans };
    }

}


#endif /*__WHIRL_HPP__*/
