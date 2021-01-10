/******************************************
Copyright (C) 2009-2020 Authors of CryptoMiniSat, see AUTHORS file

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
***********************************************/


#ifndef CLAUSE_H
#define CLAUSE_H

#include <cstdio>
#include <vector>
#include <sys/types.h>
#include <string.h>
#include <limits>

#include "solverconf.h"
#include "solvertypes.h"
#include "constants.h"
#include "watched.h"
#include "alg.h"
#include "clabstraction.h"
#include "avgcalc.h"
#include "constants.h"

namespace CMSat {

class ClauseAllocator;

/**
@brief Decides only using abstraction if clause A could subsume clause B

@note: It can give false positives. Never gives false negatives.

For A to subsume B, everything that is in A MUST be in B. So, if (A & ~B)
contains even one bit, it means that A contains something that B doesn't. So
A may be a subset of B only if (A & ~B) == 0
*/
inline bool subsetAbst(const cl_abst_type A, const cl_abst_type B)
{
    return ((A & ~B) == 0);
}

template <class T>
struct AtecedentData
{
    void clear()
    {
        *this = AtecedentData<T>();
    }

    uint64_t num() const
    {
        return binRed + binIrred + longIrred + longRed;
    }

    template<class T2>
    AtecedentData& operator+=(const AtecedentData<T2>& other)
    {
        binRed += other.binRed;
        binIrred += other.binIrred;
        longIrred += other.longIrred;
        longRed += other.longRed;

        glue_long_reds += other.glue_long_reds;
        size_longs += other.size_longs;
        age_long_reds += other.age_long_reds;

        return *this;
    }

    template<class T2>
    AtecedentData& operator-=(const AtecedentData<T2>& other)
    {
        binRed -= other.binRed;
        binIrred -= other.binIrred;
        longIrred -= other.longIrred;
        longRed -= other.longRed;

        glue_long_reds -= other.glue_long_reds;
        size_longs -= other.size_longs;
        age_long_reds -= other.age_long_reds;

        return *this;
    }

    uint32_t sum_size() const
    {
        uint32_t sum = 0;
        sum += binIrred*2;
        sum += binRed*2;
        sum += size_longs.get_sum();

        return sum;
    }

    T binRed = 0;
    T binIrred = 0;
    T longIrred = 0;
    T longRed = 0;
    AvgCalc<uint32_t> glue_long_reds;
    AvgCalc<uint32_t> size_longs;
    AvgCalc<uint32_t> age_long_reds;
};

struct RDBExtraData {
    uint32_t props_made_rank;
    uint32_t act_rank;
    uint32_t uip1_used_rank;
    double pred_short_use;
    double pred_long_use;
    double pred_forever_use;
};

struct ClauseStats
{
    ClauseStats()
    {
        //NOTE: we *MUST* set values to high default, as we do
        //combineStats(default, newclause) to get combined stats.
        glue = 1000;
        is_decision = false;
        marked_clause = false;
        ttl = 0;
        which_red_array = 2;
        locked_for_data_gen = 0;
        is_ternary_resolvent = 0;
        activity = 0;
    }

    //Stored data
    uint32_t glue:20;  //currently in code limited to 100'000
    uint32_t is_decision:1; //a "decision clause", i.e. made out of decisions leading to conflict, not resolution
    uint32_t marked_clause:1;
    uint32_t ttl:1;
    uint32_t which_red_array:3;
    uint32_t locked_for_data_gen:1;
    uint32_t is_ternary_resolvent:1;
    union {
        float   activity;
        uint32_t hash_val; //used in BreakID to remove equivalent clauses
    };
    uint32_t last_touched = 0;
    #ifdef FINAL_PREDICTOR
    float       glueHist_longterm_avg;
    float       glueHist_avg;
    uint32_t    glue_before_minim;
    float       overlapHistLT_avg;
    uint32_t    num_total_lits_antecedents;
    uint32_t    num_antecedents;
    float       numResolutionsHistLT_avg;
    float       conflSizeHist_avg;
    float       glueHistLT_avg;
    #endif

    #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR)
    uint32_t orig_glue = 1000;
    uint32_t introduced_at_conflict = 0; ///<At what conflict number the clause  was introduced
    float discounted_props_made = 0;
    float discounted_uip1_used = 0;
    uint32_t sum_uip1_used = 0; ///N.o. times claue was used during 1st UIP generation for ALL TIME
    uint32_t sum_props_made = 0; ///<Number of times caused propagation
    uint32_t pred_clnum;
    #endif

    #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR) || defined(NORMAL_CL_USE_STATS)
    uint32_t uip1_used = 0; ///N.o. times claue was used during 1st UIP generation in this RDB
    uint32_t props_made = 0; ///<Number of times caused propagation
    #if defined(STATS_NEEDED) || defined(NORMAL_CL_USE_STATS)
    uint32_t clause_looked_at = 0; ///<Number of times the clause has been deferenced during propagation
    #endif
    #endif

    #ifdef STATS_NEEDED
    int32_t ID = 0;
    uint32_t dump_no = 0;
    uint32_t orig_connects_num_communities = 0;
    uint32_t connects_num_communities = 0;
    float discounted_uip1_used2 = 0;
    float discounted_uip1_used3 = 0;
    float discounted_props_made2 = 0;
    float discounted_props_made3 = 0;
    uint32_t conflicts_made = 0; ///<Number of times caused conflict
    uint32_t ttl_stats = 0;

    AtecedentData<uint16_t> antec_data;
    #endif

    #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR)
    void reset_rdb_stats()
    {
        float discount_factor;
        discount_factor = 0.8;
        discounted_props_made *= discount_factor;
        discounted_props_made += (float)props_made*(1.0f-discount_factor);

        discount_factor = 0.8;
        discounted_uip1_used *= discount_factor;
        discounted_uip1_used += (float)uip1_used*(1.0f-discount_factor);

        #ifdef STATS_NEEDED
        discount_factor = 0.90;
        discounted_uip1_used3 *= discount_factor;
        discounted_uip1_used3 += (float)uip1_used*(1.0f-discount_factor);

        discount_factor = 0.4;
        discounted_props_made2 *= discount_factor;
        discounted_props_made2 += (float)props_made*(1.0f-discount_factor);

        discount_factor = 0.4;
        discounted_uip1_used2 *= discount_factor;
        discounted_uip1_used2 += (float)uip1_used*(1.0f-discount_factor);

        discount_factor = 0.90;
        discounted_props_made3 *= discount_factor;
        discounted_props_made3 += (float)props_made*(1.0f-discount_factor);

        antec_data.clear();
        conflicts_made = 0;
        ttl_stats = 0;
        clause_looked_at = 0;
        #endif
        uip1_used = 0;
        props_made = 0;
    }
    #endif

    #if defined(NORMAL_CL_USE_STATS)
    void reset_rdb_stats()
    {
        uip1_used = 0;
        props_made = 0;
        clause_looked_at = 0;

    }
    #endif


    static ClauseStats combineStats(const ClauseStats& first, const ClauseStats& second)
    {
        //Create to-be-returned data
        ClauseStats ret = first;

        //Combine stats
        ret.glue = std::min(first.glue, second.glue);
        ret.activity = std::max(first.activity, second.activity);
        ret.last_touched = std::max(first.last_touched, second.last_touched);
        ret.locked_for_data_gen = std::max(first.locked_for_data_gen, second.locked_for_data_gen);
        ret.is_ternary_resolvent = first.is_ternary_resolvent;

        #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR)
        if (first.introduced_at_conflict == 0) {
            ret.introduced_at_conflict = second.introduced_at_conflict;
        } else if (second.introduced_at_conflict == 0) {
            ret.introduced_at_conflict = first.introduced_at_conflict;
        } else {
            ret.introduced_at_conflict = std::min(first.introduced_at_conflict, second.introduced_at_conflict);
        }
        ret.sum_uip1_used = first.sum_uip1_used + second.sum_uip1_used;
        ret.sum_props_made = first.sum_props_made + second.sum_props_made;
        ret.discounted_props_made = first.discounted_props_made + second.discounted_props_made;
        ret.discounted_uip1_used =   first.discounted_uip1_used   + second.discounted_uip1_used;
        ret.orig_glue = std::min(first.orig_glue, second.orig_glue);
        #endif

        #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR) || defined(NORMAL_CL_USE_STATS)
        ret.uip1_used = first.uip1_used + second.uip1_used;
        ret.props_made = first.props_made + second.props_made;
        #endif

        #ifdef STATS_NEEDED
        ret.clause_looked_at = first.clause_looked_at + second.clause_looked_at;
        ret.dump_no = std::max(first.dump_no, second.dump_no);
        ret.ttl_stats = std::max(first.ttl_stats, second.ttl_stats);
        ret.conflicts_made = first.conflicts_made + second.conflicts_made;
        ret.discounted_uip1_used3 = first.discounted_uip1_used3 + second.discounted_uip1_used3;
        ret.discounted_props_made2 = first.discounted_props_made2 + second.discounted_props_made2;
        ret.discounted_props_made3 = first.discounted_props_made3 + second.discounted_props_made3;
        ret.discounted_uip1_used2 =  first.discounted_uip1_used2  + second.discounted_uip1_used2;
        #endif

        #ifdef FINAL_PREDICTOR
        ret.which_red_array = std::min(first.which_red_array, second.which_red_array);
        #endif

        return ret;
    }
};

inline std::ostream& operator<<(std::ostream& os, const ClauseStats& stats)
{

    os << "glue " << stats.glue << " ";
    #if defined(STATS_NEEDED) || defined (FINAL_PREDICTOR)
    os << "conflIntro " << stats.introduced_at_conflict<< " ";
    os << "uip1_used " << stats.uip1_used << " ";
    os << "numProp " << stats.props_made<< " ";
    #endif
    #ifdef STATS_NEEDED
    os << "numConfl " << stats.conflicts_made<< " ";
    os << "numLook " << stats.clause_looked_at<< " ";
    #endif

    return os;
}

/**
@brief Holds a clause. Does not allocate space for literals

Literals are allocated by an external allocator that allocates enough space
for the class that it can hold the literals as well. I.e. it malloc()-s
    sizeof(Clause)+LENGHT*sizeof(Lit)
to hold the clause.
*/
class Clause
{
public:
    ClauseStats stats;
    uint16_t isRed:1; ///<Is the clause a redundant clause?
    uint16_t isRemoved:1; ///<Is this clause queued for removal?
    uint16_t isFreed:1; ///<Has this clause been marked as freed by the ClauseAllocator ?
    uint16_t distilled:1;
    uint16_t is_ternary_resolved:1;
    uint16_t occurLinked:1;
    uint16_t must_recalc_abst:1;
    uint16_t _used_in_xor:1;
    uint16_t _used_in_xor_full:1;
    uint16_t _xor_is_detached:1;
    uint16_t _gauss_temp_cl:1; ///Used ONLY by Gaussian elimination to incicate where a proagation is coming from
    uint16_t reloced:1;
    uint16_t disabled:1;
    uint16_t marked:1;
    uint16_t tried_to_remove:1;


    Lit* getData()
    {
        return (Lit*)((char*)this + sizeof(Clause));
    }

    const Lit* getData() const
    {
        return (Lit*)((char*)this + sizeof(Clause));
    }

public:
    cl_abst_type abst;
    uint32_t mySize;

    template<class V>
    Clause(const V& ps, const uint32_t _introduced_at_conflict
        #ifdef STATS_NEEDED
        , const int64_t _ID
        #endif
        )
    {
        //assert(ps.size() > 2);

        stats.last_touched = _introduced_at_conflict;
        #if defined(FINAL_PREDICTOR) || defined(STATS_NEEDED)
        stats.introduced_at_conflict = _introduced_at_conflict;
        #endif

        #ifdef STATS_NEEDED
        stats.ID = _ID;
        assert(_ID >= 0);
        #endif
        isFreed = false;
        mySize = ps.size();
        isRed = false;
        isRemoved = false;
        distilled = false;
        is_ternary_resolved = false;
        must_recalc_abst = true;
        _used_in_xor = false;
        _used_in_xor_full = false;
        _xor_is_detached = false;
        reloced = false;
        disabled = false;
        marked = false;
        tried_to_remove = false;

        for (uint32_t i = 0; i < ps.size(); i++) {
            getData()[i] = ps[i];
        }
    }

    typedef Lit* iterator;
    typedef const Lit* const_iterator;

    uint32_t size() const
    {
        return mySize;
    }

    bool used_in_xor() const
    {
        return _used_in_xor;
    }

    void set_used_in_xor(const bool val)
    {
        _used_in_xor = val;
    }

    bool used_in_xor_full() const
    {
        return _used_in_xor_full;
    }

    void set_used_in_xor_full(const bool val)
    {
        _used_in_xor_full = val;
    }

    void shrink(const uint32_t i)
    {
        assert(i <= size());
        mySize -= i;
        if (i > 0)
            setStrenghtened();
    }

    void resize (const uint32_t i)
    {
        assert(i <= size());
        if (i == size()) return;
        mySize = i;
        setStrenghtened();
    }

    bool red() const
    {
        return isRed;
    }

    bool freed() const
    {
        return isFreed;
    }

    void reCalcAbstraction()
    {
        abst = calcAbstraction(*this);
        must_recalc_abst = false;
    }

    void setStrenghtened()
    {
        must_recalc_abst = true;
        //is_ternary_resolved = false; //probably not a good idea
        //is_distilled = false; //TODO?
    }

    void recalc_abst_if_needed()
    {
        if (must_recalc_abst) {
            reCalcAbstraction();
        }
    }

    Lit& operator [] (const uint32_t i)
    {
        return *(getData() + i);
    }

    const Lit& operator [] (const uint32_t i) const
    {
        return *(getData() + i);
    }

    void makeIrred()
    {
        assert(isRed);
        #if STATS_NEEDED
        stats.ID = 0;
        #endif
        isRed = false;
    }

    void makeRed(uint64_t
    #if defined(FINAL_PREDICTOR) || defined(STATS_NEEDED)
    confl_num
    #endif
    )
    {
        isRed = true;
        #if defined(FINAL_PREDICTOR) || defined(STATS_NEEDED)
        stats.introduced_at_conflict = confl_num;
        #endif
    }

    void strengthen(const Lit p)
    {
        remove(*this, p);
        setStrenghtened();
    }

    void add(const Lit p)
    {
        mySize++;
        getData()[mySize-1] = p;
        setStrenghtened();
    }

    const Lit* begin() const
    {
        #ifdef SLOW_DEBUG
        assert(!freed());
        assert(!getRemoved());
        #endif
        return getData();
    }

    Lit* begin()
    {
        #ifdef SLOW_DEBUG
        assert(!freed());
        assert(!getRemoved());
        #endif
        return getData();
    }

    const Lit* end() const
    {
        return getData()+size();
    }

    Lit* end()
    {
        return getData()+size();
    }

    void setRemoved()
    {
        isRemoved = true;
    }

    bool getRemoved() const
    {
        return isRemoved;
    }

    void unset_removed()
    {
        isRemoved = false;
    }

    void setFreed()
    {
        isFreed = true;
    }

    void combineStats(const ClauseStats& other)
    {
        stats = ClauseStats::combineStats(stats, other);
    }

    bool getOccurLinked() const
    {
        return occurLinked;
    }

    void setOccurLinked(bool toset)
    {
        occurLinked = toset;
    }

    void print_extra_stats() const
    {
        cout
        << "Clause size " << std::setw(4) << size();
        if (red()) {
            cout << " glue : " << std::setw(4) << stats.glue;
        }
        #ifdef STATS_NEEDED
        cout
        << " Confls: " << std::setw(10) << stats.conflicts_made
        << " Props: " << std::setw(10) << stats.props_made
        << " Looked at: " << std::setw(10)<< stats.clause_looked_at
        << " UIP used: " << std::setw(10)<< stats.uip1_used;
        #endif
        cout << endl;
    }

    void copy_to(vector<Lit>& lits) {
        lits.clear();
        for(const Lit l: *this) {
            lits.push_back(l);
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, const Clause& cl)
{
    for (uint32_t i = 0; i < cl.size(); i++) {
        os << cl[i];

        if (i+1 != cl.size())
            os << " ";
    }

    return os;
}

struct BinaryXor
{
    uint32_t vars[2];
    bool rhs;

    BinaryXor(uint32_t var1, uint32_t var2, const bool _rhs) {
        if (var1 > var2) {
            std::swap(var1, var2);
        }
        vars[0] = var1;
        vars[1] = var2;
        rhs = _rhs;
    }

    bool operator<(const BinaryXor& other) const
    {
        if (vars[0] != other.vars[0]) {
            return vars[0] < other.vars[0];
        }

        if (vars[1] != other.vars[1]) {
            return vars[1] < other.vars[1];
        }

        if (rhs != other.rhs) {
            return (int)rhs < (int)other.rhs;
        }
        return false;
    }
};

} //end namespace

#endif //CLAUSE_H
