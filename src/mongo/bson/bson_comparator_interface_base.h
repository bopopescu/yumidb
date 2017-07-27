/**
 *    Copyright (C) 2016 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <initializer_list>
#include <map>
#include <set>

#include "mongo/base/disallow_copying.h"
#include "mongo/base/string_data_comparator_interface.h"
#include "mongo/stdx/unordered_map.h"
#include "mongo/stdx/unordered_set.h"
#include "mongo/util/assert_util.h"

namespace mongo {

class BSONElement;
class BSONObj;

/**
 * Base class for the BSONObj and BSONElement comparator interfaces.
 */
template <typename T>
class BSONComparatorInterfaceBase {
    MONGO_DISALLOW_COPYING(BSONComparatorInterfaceBase);

public:
    /**
     * A deferred comparison between two objects of type T, which can be converted into a boolean
     * via the evaluate() method.
     */
    struct DeferredComparison {
        enum class Type {
            kLT,
            kLTE,
            kEQ,
            kGT,
            kGTE,
            kNE,
        };

        DeferredComparison(Type type, const T& lhs, const T& rhs)
            : type(type), lhs(lhs), rhs(rhs) {}

        Type type;
        const T& lhs;
        const T& rhs;
    };

    /**
     * Functor compatible for use with ordered STL containers.
     */
    class LessThan {
    public:
        explicit LessThan(const BSONComparatorInterfaceBase* comparator)
            : _comparator(comparator) {}

        bool operator()(const T& lhs, const T& rhs) const {
            return _comparator->compare(lhs, rhs) < 0;
        }

    private:
        const BSONComparatorInterfaceBase* _comparator;
    };

    /**
     * Functor compatible for use with unordered STL containers.
     */
    class EqualTo {
    public:
        explicit EqualTo(const BSONComparatorInterfaceBase* comparator) : _comparator(comparator) {}

        bool operator()(const T& lhs, const T& rhs) const {
            return _comparator->compare(lhs, rhs) == 0;
        }

    private:
        const BSONComparatorInterfaceBase* _comparator;
    };

    /**
     * Function object for hashing with respect to this comparator. Compatible with the hash concept
     * from std::hash.
     */
    class Hasher {
    public:
        explicit Hasher(const BSONComparatorInterfaceBase* comparator) : _comparator(comparator) {}

        size_t operator()(const T& toHash) const {
            return _comparator->hash(toHash);
        }

    private:
        const BSONComparatorInterfaceBase* _comparator;
    };

    using Set = std::set<T, LessThan>;

    using UnorderedSet = stdx::unordered_set<T, Hasher, EqualTo>;

    template <typename ValueType>
    using Map = std::map<T, ValueType, LessThan>;

    template <typename ValueType>
    using UnorderedMap = stdx::unordered_map<T, ValueType, Hasher, EqualTo>;

    virtual ~BSONComparatorInterfaceBase() = default;

    /**
     * Compares two BSONObj/BSONElement objects. Returns <0, 0, >0 if 'lhs' < 'rhs', 'lhs' == 'rhs',
     * or 'lhs' > 'rhs' respectively.
     */
    virtual int compare(const T& lhs, const T& rhs) const = 0;

    /**
     * Produces a hash for a BSONObj or BSONElement in such a way that respects this comparator.
     *
     * The hash function is subject to change. Do not use in cases where hashes need to be
     * consistent across versions.
     */
    size_t hash(const T& toHash) const {
        size_t seed = 0;
        hash_combine(seed, toHash);
        return seed;
    }

    /**
     * Produces a hash for a BSONObj or BSONElement in such a way that respects this comparator. The
     * resulting hash is combined with 'seed', allowing the caller to create a composite hash out of
     * several BSONObj/BSONElement objects.
     *
     * The hash function is subject to change. Do not use in cases where hashes need to be
     * consistent across versions.
     */
    virtual void hash_combine(size_t& seed, const T& toHash) const = 0;

    /**
     * Evaluates a deferred comparison object generated by invocation of one of the BSONObj operator
     * overloads for relops.
     */
    bool evaluate(DeferredComparison deferredComparison) const {
        int cmp = compare(deferredComparison.lhs, deferredComparison.rhs);
        switch (deferredComparison.type) {
            case DeferredComparison::Type::kLT:
                return cmp < 0;
            case DeferredComparison::Type::kLTE:
                return cmp <= 0;
            case DeferredComparison::Type::kEQ:
                return cmp == 0;
            case DeferredComparison::Type::kGT:
                return cmp > 0;
            case DeferredComparison::Type::kGTE:
                return cmp >= 0;
            case DeferredComparison::Type::kNE:
                return cmp != 0;
        }

        MONGO_UNREACHABLE;
    }

    /**
     * Returns a function object which computes whether one BSONObj is less than another under this
     * comparator. This comparator must outlive the returned function object.
     */
    LessThan makeLessThan() const {
        return LessThan(this);
    }

    /**
     * Returns a function object which computes whether one BSONObj is equal to another under this
     * comparator. This comparator must outlive the returned function object.
     */
    EqualTo makeEqualTo() const {
        return EqualTo(this);
    }

protected:
    constexpr BSONComparatorInterfaceBase() = default;

    Set makeSet(std::initializer_list<T> init = {}) const {
        return Set(init, LessThan(this));
    }

    UnorderedSet makeUnorderedSet(std::initializer_list<T> init = {}) const {
        return UnorderedSet(init, 0, Hasher(this), EqualTo(this));
    }

    template <typename ValueType>
    Map<ValueType> makeMap(std::initializer_list<std::pair<const T, ValueType>> init = {}) const {
        return Map<ValueType>(init, LessThan(this));
    }

    template <typename ValueType>
    UnorderedMap<ValueType> makeUnorderedMap(
        std::initializer_list<std::pair<const T, ValueType>> init = {}) const {
        return UnorderedMap<ValueType>(init, 0, Hasher(this), EqualTo(this));
    }

    /**
     * Hashes 'objToHash', respecting the equivalence classes given by 'stringComparator'.
     *
     * Helper intended for use by subclasses.
     */
    static void hashCombineBSONObj(size_t& seed,
                                   const BSONObj& objToHash,
                                   bool considerFieldName,
                                   const StringData::ComparatorInterface* stringComparator);

    /**
     * Hashes 'elemToHash', respecting the equivalence classes given by 'stringComparator'.
     *
     * Helper intended for use by subclasses.
     */
    static void hashCombineBSONElement(size_t& seed,
                                       BSONElement elemToHash,
                                       bool considerFieldName,
                                       const StringData::ComparatorInterface* stringComparator);
};

}  // namespace mongo
