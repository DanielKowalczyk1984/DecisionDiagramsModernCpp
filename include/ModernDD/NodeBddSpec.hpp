/*
 * TdZdd: a Top-down/Breadth-first Decision Diagram Manipulation Framework
 * by Hiroaki Iwashita <iwashita@erato.ist.hokudai.ac.jp>
 * Copyright (c) 2014 ERATO MINATO Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NODE_BDD_SPEC_HPP
#define NODE_BDD_SPEC_HPP

#include <cassert>   // for assert
#include <cstddef>   // for size_t
#include <cstdint>   // for uint16_t
#include <iostream>  // for ostream, operator<<, basic_ostream:...
#include <new>       // for operator new
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/to_container.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>  // for zip
#include <span>
#include <stdexcept>  // for runtime_error
#include <string>     // for allocator, string

#include "NodeBddDumper.hpp"  // for DdDumper

/**
 * Base class of DD specs.
 *
 * Every implementation must have the following functions:
 * - int datasize() const
 * - int get_root(void* p)
 * - int get_child(void* p, int level, int value)
 * - void get_copy(void* to, void const* from)
 * - int merge_states(void* p1, void* p2)
 * - void destruct(void* p)
 * - void destructLevel(int level)
 * - size_t hash_code(void const* p, int level) const
 * - bool equal_to(void const* p, void const* q, int level) const
 * - void print_state(std::ostream& os, void const* p, int level) const
 *
 * Optionally, the following functions can be overloaded:
 * - void printLevel(std::ostream& os, int level) const
 *
 * A return code of get_root(void*) or get_child(void*, int, bool) is:
 * 0 when the node is the 0-terminal, -1 when it is the 1-terminal, or
 * the node level when it is a non-terminal.
 * A return code of merge_states(void*, void*) is: 0 when the states are
 * merged into the first one, 1 when they cannot be merged and the first
 * one should be forwarded to the 0-terminal, 2 when they cannot be merged
 * and the second one should be forwarded to the 0-terminal.
 *
 * @tparam S the class implementing this class.
 * @tparam AR arity of the nodes.
 */
template <typename S, size_t AR = 2>
class DdSpecBase {
   public:
    static size_t const ARITY = AR;

    S& entity() { return *static_cast<S*>(this); }

    S const& entity() const { return *static_cast<S const*>(this); }

    void printLevel(std::ostream& os, int level) const { os << level; }

    /**
     * Returns a random instance using simple depth-first search
     * without caching.
     * It does not guarantee that the selection is uniform.
     * merge_states(void*, void*) is not supported.
     * @return a collection of (item, value) pairs.
     * @exception std::runtime_error no instance exists.
     */
    // std::vector<std::pair<int,int> > findOneInstance() const {
    // return DepthFirstSearcher<S>(entity()).findOneInstance();
    // }

    /**
     * Dumps the node table in Graphviz (dot) format.
     * @param os the output stream.
     * @param title title label.
     */
    void dumpDot(std::ostream& os = std::cout,
                 std::string   title = "Bdd") const {
        DdDumper<S> dumper(entity());
        dumper.dump(os, title);
    }

    /**
     * Dumps the node table in Graphviz (dot) format.
     * @param os the output stream.
     * @param o the ZDD.
     * @return os itself.
     */
    friend std::ostream& operator<<(std::ostream& os, DdSpecBase const& o) {
        o.dumpDot(os);
        return os;
    }

   private:
    static constexpr int HASH_CONST_BASE = 314159257;
    template <typename T, typename I>
    static size_t rawHashCode_(void const* p) {
        size_t h = 0;
        // auto*     a = static_cast<I const*>(p);
        std::span aux{static_cast<I const*>(p), sizeof(T) / sizeof(I)};
        for (auto const& it : aux) {
            h += it;
            h *= HASH_CONST_BASE;
        }
        return h;
    }

    template <typename T, typename I>
    static size_t rawEqualTo_(void const* p1, void const* p2) {
        // I const*  a1 = static_cast<I const*>(p1);
        // I const*  a2 = static_cast<I const*>(p2);
        std::span aux1{static_cast<I const*>(p1), sizeof(T) / sizeof(I)};
        std::span aux2{static_cast<I const*>(p2), sizeof(T) / sizeof(I)};

        for (auto&& [it1, it2] : ranges::views::zip(aux1, aux2)) {
            if (it1 != it2) {
                return false;
            }
        }
        return true;
    }

   protected:
    template <typename T>
    static size_t rawHashCode(T const& o) {
        if (sizeof(T) % sizeof(size_t) == 0) {
            return rawHashCode_<T, size_t>(&o);
        }
        if (sizeof(T) % sizeof(unsigned int) == 0) {
            return rawHashCode_<T, unsigned int>(&o);
        }
        if (sizeof(T) % sizeof(std::uint16_t) == 0) {
            return rawHashCode_<T, std::uint16_t>(&o);
        }
        return rawHashCode_<T, unsigned char>(&o);
    }

    template <typename T>
    static size_t rawEqualTo(T const& o1, T const& o2) {
        if (sizeof(T) % sizeof(size_t) == 0) {
            return rawEqualTo_<T, size_t>(&o1, &o2);
        }
        if (sizeof(T) % sizeof(unsigned int) == 0) {
            return rawEqualTo_<T, unsigned int>(&o1, &o2);
        }
        if (sizeof(T) % sizeof(std::uint16_t) == 0) {
            return rawEqualTo_<T, std::uint16_t>(&o1, &o2);
        }
        return rawEqualTo_<T, unsigned char>(&o1, &o2);
    }
};

/**
 * Abstract class of DD specifications without states.
 *
 * Every implementation must have the following functions:
 * - int getRoot()
 * - int getChild(int level, size_t value)
 *
 * Optionally, the following functions can be overloaded:
 * - void printLevel(std::ostream& os, int level) const
 *
 * @tparam S the class implementing this class.
 * @tparam AR arity of the nodes.
 */
template <typename S, size_t AR = 2>
class StatelessDdSpec : public DdSpecBase<S, AR> {
   public:
    [[nodiscard]] size_t datasize() const { return 0UL; }

    int get_root([[maybe_unused]] void* p) { return this->entity().getRoot(); }

    int get_child([[maybe_unused]] void* p, int level, size_t value) {
        assert(value < S::ARITY);
        return this->entity().getChild(level, value);
    }

    void get_copy([[maybe_unused]] void*       to,
                  [[maybe_unused]] void const* from) {}

    int merge_states([[maybe_unused]] void* p1, [[maybe_unused]] void* p2) {
        return 0;
    }

    void destruct([[maybe_unused]] void* p) {}

    // void destructLevel(int level) {}

    size_t hash_code([[maybe_unused]] void const* p,
                     [[maybe_unused]] int         level) const {
        return 0;
    }

    bool equal_to([[maybe_unused]] void const* p,
                  [[maybe_unused]] void const* q,
                  [[maybe_unused]] int         level) const {
        return true;
    }

    void print_state(std::ostream&                os,
                     [[maybe_unused]] void const* p,
                     [[maybe_unused]] int         level) const {
        os << "*";
    }
};

/**
 * Abstract class of DD specifications using scalar states.
 *
 * Every implementation must have the following functions:
 * - int getRoot(T& state)
 * - int getChild(T& state, int level, size_t value)
 *
 * Optionally, the following functions can be overloaded:
 * - void construct(void* p)
 * - void getCopy(void* p, T const& state)
 * - void mergeStates(T& state1, T& state2)
 * - size_t hashCode(T const& state) const
 * - bool equalTo(T const& state1, T const& state2) const
 * - void printLevel(std::ostream& os, int level) const
 * - void printState(std::ostream& os, State const& s) const
 *
 * @tparam S the class implementing this class.
 * @tparam T data type.
 * @tparam AR arity of the nodes.
 */
template <typename S, typename T, size_t AR = 2>
class DdSpec : public DdSpecBase<S, AR> {
   public:
    using State = T;

   private:
    static State& state(void* p) { return *static_cast<State*>(p); }

    static State const& state(void const* p) {
        return *static_cast<State const*>(p);
    }

   public:
    [[nodiscard]] size_t datasize() const { return sizeof(State); }

    void construct(void* p) { new (p) State(); }

    int get_root(void* p) {
        this->entity().construct(p);
        return this->entity().getRoot(state(p));
    }

    int get_child(void* p, int level, size_t value) {
        assert(value < S::ARITY);
        return this->entity().getChild(state(p), level, value);
    }

    void getCopy(void* p, State const& s) { new (p) State(s); }

    void get_copy(void* to, void const* from) {
        this->entity().getCopy(to, state(from));
    }

    int mergeStates([[maybe_unused]] State& s1, [[maybe_unused]] State& s2) {
        return 0;
    }

    int merge_states(void* p1, void* p2) {
        return this->entity().mergeStates(state(p1), state(p2));
    }

    void destruct(void* p) { state(p).~State(); }

    // void destructLevel(int level) {}

    size_t hashCode(State const& s) const { return this->rawHashCode(s); }

    size_t hashCodeAtLevel(State const& s, [[maybe_unused]] int level) const {
        return this->entity().hashCode(s);
    }

    size_t hash_code(void const* p, int level) const {
        return this->entity().hashCodeAtLevel(state(p), level);
    }

    bool equalTo(State const& s1, State const& s2) const {
        return this->rawEqualTo(s1, s2);
    }

    bool equalToAtLevel(State const&         s1,
                        State const&         s2,
                        [[maybe_unused]] int level) const {
        return this->entity().equalTo(s1, s2);
    }

    bool equal_to(void const* p, void const* q, int level) const {
        return this->entity().equalToAtLevel(state(p), state(q), level);
    }

    void printState(std::ostream& os, State const& s) const { os << s; }

    void printStateAtLevel(std::ostream&        os,
                           State const&         s,
                           [[maybe_unused]] int level) const {
        this->entity().printState(os, s);
    }

    void print_state(std::ostream& os, void const* p, int level) const {
        this->entity().printStateAtLevel(os, state(p), level);
    }
};

/**
 * Abstract class of DD specifications using POD array states.
 * The size of array must be set by setArraySize(int n) in the constructor
 * and cannot be changed.
 * If you want some arbitrary-sized data storage for states,
 * use pointers to those storages in DdSpec instead.
 *
 * Every implementation must have the following functions:
 * - int getRoot(T* array)
 * - int getChild(T* array, int level, size_t value)
 *
 * Optionally, the following functions can be overloaded:
 * - void mergeStates(T* array1, T* array2)
 * - void printLevel(std::ostream& os, int level) const
 * - void printState(std::ostream& os, State const* array) const
 *
 * @tparam S the class implementing this class.
 * @tparam T data type of array elements.
 * @tparam AR arity of the nodes.
 */
template <typename S, typename T, size_t AR = 2>
class PodArrayDdSpec : public DdSpecBase<S, AR> {
   public:
    using State = T;

   private:
    using Word = size_t;

    int arraySize{};
    int dataWords{};

    static State* state(void* p) { return static_cast<State*>(p); }

    static State const* state(void const* p) {
        return static_cast<State const*>(p);
    }
    static constexpr size_t HASH_CONST_POD_ARRAY = 314159257;

   protected:
    void setArraySize(int n) {
        assert(0 <= n);
        if (arraySize >= 0) {
            throw std::runtime_error(
                "Cannot set array size twice; use setArraySize(int) only once "
                "in the constructor of DD spec.");
        }
        arraySize = n;
        dataWords = (n * sizeof(State) + sizeof(Word) - 1) / sizeof(Word);
    }

    [[nodiscard]] int getArraySize() const { return arraySize; }

   public:
    PodArrayDdSpec() : arraySize(-1), dataWords(-1) {}

    [[nodiscard]] size_t datasize() const {
        if (dataWords < 0) {
            throw std::runtime_error(
                "Array size is unknown; please set it by setArraySize(int) in "
                "the constructor of DD spec.");
        }
        return dataWords * sizeof(Word);
    }

    int get_root(void* p) { return this->entity().getRoot(state(p)); }

    int get_child(void* p, int level, size_t value) {
        assert(value < S::ARITY);
        return this->entity().getChild(state(p), level, value);
    }

    void get_copy(void* to, void const* from) {
        Word const* pa = static_cast<Word const*>(from);
        // Word const* pz = pa + dataWords;
        Word* qa = static_cast<Word*>(to);
        auto  aux_p = std::span<Word const>{pa, dataWords};
        auto  aux_q = std::span<Word>{qa, dataWords};
        ranges::copy(aux_p.begin(), aux_p.end(), aux_q.begin());
        // while (pa != pz)
        // {
        //   *qa++ = *pa++;
        // }
    }

    int mergeStates([[maybe_unused]] T* a1, [[maybe_unused]] T* a2) {
        return 0;
    }

    int merge_states(void* p1, void* p2) {
        return this->entity().mergeStates(state(p1), state(p2));
    }

    void destruct([[maybe_unused]] void* p) {}

    // void destructLevel(int level) {}

    size_t hash_code(void const* p, [[maybe_unused]] int level) const {
        Word const* pa = static_cast<Word const*>(p);
        // Word const* pz = pa + dataWords;
        std::span<Word const> aux{pa, size_t(dataWords)};
        // size_t h = 0;
        return ranges::accumulate(aux, 0UL, [](auto a, auto b) {
            auto tmp{a + b};
            tmp *= HASH_CONST_POD_ARRAY;
            return tmp;
        });
        // while (pa != pz) {
        //     h += *pa++;
        //     h *= HASH_CONST_POD_ARRAY;
        // }
        // return h;
    }

    bool equal_to(void const*          p,
                  void const*          q,
                  [[maybe_unused]] int level) const {
        Word const* pa = static_cast<Word const*>(p);
        Word const* qa = static_cast<Word const*>(q);
        // Word const* pz = pa + dataWords;
        auto aux_p = std::span<Word const>(pa, size_t(dataWords));
        auto aux_q = std::span<Word const>(qa, size_t(dataWords));
        // while (pa != pz) {
        //     if (*pa++ != *qa++) {
        //         return false;
        //     }
        // }
        // return true;
        return ranges::equal(aux_p, aux_q);
    }

    void printState(std::ostream& os, State const* a) const {
        auto aux = std::span<State const>(a, size_t(arraySize));
        auto joined = aux | ranges::views::transform([](auto& tmp) {
                          return std::to_string(tmp);
                      }) |
                      ranges::views::join(", ");

        os << "[" << ranges::to_<std::string>(joined) << os << "]\n";
    }

    void printStateAtLevel(std::ostream&        os,
                           State const*         a,
                           [[maybe_unused]] int level) const {
        this->entity().printState(os, a);
    }

    void print_state(std::ostream& os, void const* p, int level) const {
        this->entity().printStateAtLevel(os, state(p), level);
    }
};

/**
 * Abstract class of DD specifications using both scalar and POD array states.
 *
 * Every implementation must have the following functions:
 * - int getRoot(TS& scalar, TA* array)
 * - int getChild(TS& scalar, TA* array, int level, size_t value)
 *
 * Optionally, the following functions can be overloaded:
 * - void construct(void* p)
 * - void getCopy(void* p, TS const& state)
 * - void mergeStates(TS& s1, TA* a1, TS& s2, TA* a2)
 * - size_t hashCode(TS const& state) const
 * - bool equalTo(TS const& state1, TS const& state2) const
 * - void printLevel(std::ostream& os, int level) const
 * - void printState(std::ostream& os, TS const& s, TA const* a) const
 *
 * @tparam S the class implementing this class.
 * @tparam TS data type of scalar.
 * @tparam TA data type of array elements.
 * @tparam AR arity of the nodes.
 */
template <typename S, typename TS, typename TA, size_t AR = 2>
class HybridDdSpec : public DdSpecBase<S, AR> {
   public:
    using S_State = TS;
    using A_State = TA;

   private:
    using Word = size_t;
    static int const S_WORDS =
        (sizeof(S_State) + sizeof(Word) - 1) / sizeof(Word);
    static constexpr size_t HASH_HYBRID_SPEC1 = 271828171;
    static constexpr size_t HASH_HYBRID_SPEC2 = 314159257;

    int arraySize{};
    int dataWords{};

    static S_State& s_state(void* p) { return *static_cast<S_State*>(p); }

    static S_State const& s_state(void const* p) {
        return *static_cast<S_State const*>(p);
    }

    A_State* a_state(void* p) {
        auto aux = std::span<Word>{static_cast<Word*>(p), dataWords};
        return reinterpret_cast<A_State*>(aux.subspan(S_WORDS).data());
    }

    A_State const* a_state(void const* p) {
        auto aux =
            std::span<Word const>{static_cast<Word const*>(p), dataWords};
        return reinterpret_cast<A_State const*>(aux.subspan(S_WORDS).data());
    }

   protected:
    void setArraySize(int n) {
        assert(0 <= n);
        arraySize = n;
        dataWords =
            S_WORDS + (n * sizeof(A_State) + sizeof(Word) - 1) / sizeof(Word);
    }

    [[nodiscard]] int getArraySize() const { return arraySize; }

   public:
    HybridDdSpec() : arraySize(-1), dataWords(-1) {}

    [[nodiscard]] size_t datasize() const { return dataWords * sizeof(Word); }

    void construct(void* p) { new (p) S_State(); }

    int get_root(void* p) {
        this->entity().construct(p);
        return this->entity().getRoot(s_state(p), a_state(p));
    }

    int get_child(void* p, int level, size_t value) {
        assert(0 <= value && value < S::ARITY);
        return this->entity().getChild(s_state(p), a_state(p), level, value);
    }

    void getCopy(void* p, S_State const& s) { new (p) S_State(s); }

    void get_copy(void* to, void const* from) {
        this->entity().getCopy(to, s_state(from));
        auto const* pa = static_cast<Word const*>(from);

        // auto const* pz = pa + dataWords;
        auto* qa = static_cast<Word*>(to);
        // pa += S_WORDS;
        // qa += S_WORDS;
        auto aux_p = std::span<Word const>{pa, dataWords} |
                     ranges::views::drop(size_t(S_WORDS));
        auto aux_q = std::span<Word>{qa, dataWords} |
                     ranges::views::drop(size_t(S_WORDS));
        ranges::copy(aux_p.begin(), aux_p.end(), aux_q.end());
        // while (pa != pz)
        // {
        //   *qa++ = *pa++;
        // }
    }

    int mergeStates([[maybe_unused]] S_State& s1,
                    [[maybe_unused]] A_State* a1,
                    [[maybe_unused]] S_State& s2,
                    [[maybe_unused]] A_State* a2) {
        return 0;
    }

    int merge_states(void* p1, void* p2) {
        return this->entity().mergeStates(s_state(p1), a_state(p1), s_state(p2),
                                          a_state(p2));
    }

    void destruct([[maybe_unused]] void* p) {}

    // void destructLevel(int level) {}

    size_t hashCode(S_State const& s) const { return this->rawHashCode(s); }

    size_t hashCodeAtLevel(S_State const& s, [[maybe_unused]] int level) const {
        return this->entity().hashCode(s);
    }

    size_t hash_code(void const* p, int level) const {
        size_t h = this->entity().hashCodeAtLevel(s_state(p), level);
        h *= HASH_HYBRID_SPEC1;
        Word const* pa = static_cast<Word const*>(p);
        // Word const* pz = pa + dataWords;
        // pa += S_WORDS;
        // while (pa != pz)
        // {
        //   h += *pa++;
        //   h *= HASH_HYBRID_SPEC2;
        // }

        std::span<Word const> aux{pa, dataWords};
        return ranges::accumulate(aux | ranges::views::drop(size_t(S_WORDS)), h,
                                  [](auto a, auto b) {
                                      auto tmp{a + b};
                                      tmp *= HASH_HYBRID_SPEC2;
                                      return tmp;
                                  });
    }

    bool equalTo(S_State const& s1, S_State const& s2) const {
        return this->rawEqualTo(s1, s2);
    }

    bool equalToAtLevel(S_State const&       s1,
                        S_State const&       s2,
                        [[maybe_unused]] int level) const {
        return this->entity().equalTo(s1, s2);
    }

    bool equal_to(void const* p, void const* q, int level) const {
        if (!this->entity().equalToAtLevel(s_state(p), s_state(q), level)) {
            return false;
        }
        Word const* pa = static_cast<Word const*>(p);
        Word const* qa = static_cast<Word const*>(q);
        auto        aux_p =
            std::span{pa, dataWords} | ranges::views::drop(size_t(S_WORDS));
        auto aux_q =
            std::span{qa, dataWords} | ranges::views::drop(size_t(S_WORDS));
        // Word const* pz = pa + dataWords;
        // pa += S_WORDS;
        // qa += S_WORDS;
        // while (pa != pz)
        // {
        //   if (*pa++ != *qa++)
        //   {
        //     return false;
        //   }
        // }
        return ranges::equal(aux_p, aux_q);
    }

    void printState(std::ostream&  os,
                    S_State const& s,
                    A_State const* a) const {
        auto aux = std::span<A_State const>(a, size_t(arraySize));
        auto joined = aux | ranges::views::transform([](auto& tmp) {
                          return std::to_string(tmp);
                      }) |
                      ranges::views::join(", ");

        os << "[" << s << ":" << ranges::to_<std::string>(joined) << os
           << "]\n";

        // os << "[" << s << ":";
        // for (int i = 0; i < arraySize; ++i)
        // {
        //   if (i > 0)
        //   {
        //     os << ",";
        //   }
        //   os << a[i];
        // }
        // os << "]";
    }

    void printStateAtLevel(std::ostream&        os,
                           S_State const&       s,
                           A_State const*       a,
                           [[maybe_unused]] int level) const {
        this->entity().printState(os, s, a);
    }

    void print_state(std::ostream& os, void const* p, int level) const {
        this->entity().printStateAtLevel(os, s_state(p), a_state(p), level);
    }
};

/* for backward compatibility */
template <typename S, typename TS, typename TA, size_t AR = 2>
class PodHybridDdSpec : public HybridDdSpec<S, TS, TA, AR> {};

#endif  // NODE_BDD_SPEC_HPP
