/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <third_party/murmurhash3/MurmurHash3.h>

#include <bitset>
#include <boost/intrusive_ptr.hpp>

#include "mongo/base/static_assert.h"
#include "mongo/db/pipeline/value.h"
#include "mongo/util/intrusive_counter.h"

namespace mongo {
/** Helper class to make the position in a document abstract
 *  Warning: This is NOT guaranteed to be the ordered position.
 *           eg. the first field may not be at Position(0)
 */
class Position {
public:
    // This represents "not found" similar to std::string::npos
    Position() : index(static_cast<unsigned>(-1)) {}
    bool found() const {
        return index != Position().index;
    }

    bool operator==(Position rhs) const {
        return this->index == rhs.index;
    }
    bool operator!=(Position rhs) const {
        return !(*this == rhs);
    }

    // For debugging and ASSERT_EQUALS in tests.
    template <typename OStream>
    friend OStream& operator<<(OStream& stream, Position p) {
        return stream << p.index;
    }

private:
    explicit Position(size_t i) : index(i) {}
    unsigned index;
    friend class DocumentStorage;
    friend class DocumentStorageIterator;
};

#pragma pack(1)
/** This is how values are stored in the DocumentStorage buffer
 *  Internal class. Consumers shouldn't care about this.
 */
class ValueElement {
    ValueElement(const ValueElement&) = delete;
    ValueElement& operator=(const ValueElement&) = delete;

public:
    Value val;
    Position nextCollision;  // Position of next field with same hashBucket
    const int nameLen;       // doesn't include '\0'
    const char _name[1];     // pointer to start of name (use nameSD instead)

    ValueElement* next() {
        return align(plusBytes(sizeof(ValueElement) + nameLen));
    }

    const ValueElement* next() const {
        return align(plusBytes(sizeof(ValueElement) + nameLen));
    }

    StringData nameSD() const {
        return StringData(_name, nameLen);
    }


    // helpers for doing pointer arithmetic with this class
    char* ptr() {
        return reinterpret_cast<char*>(this);
    }
    const char* ptr() const {
        return reinterpret_cast<const char*>(this);
    }
    const ValueElement* plusBytes(size_t bytes) const {
        return reinterpret_cast<const ValueElement*>(ptr() + bytes);
    }
    ValueElement* plusBytes(size_t bytes) {
        return reinterpret_cast<ValueElement*>(ptr() + bytes);
    }

    // Round number or pointer up to N-byte boundary. No change if already aligned.
    template <typename T>
    static T align(T size) {
        const intmax_t ALIGNMENT = 8;  // must be power of 2 and <= 16 (malloc alignment)
        // Can't use c++ cast because of conversion between intmax_t and both ints and pointers
        return (T)(((intmax_t)(size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1));
    }

private:
    ValueElement();   // this class should never be constructed
    ~ValueElement();  // or destructed
};
// Real size is sizeof(ValueElement) + nameLen
#pragma pack()
MONGO_STATIC_ASSERT(sizeof(ValueElement) == (sizeof(Value) + sizeof(Position) + sizeof(int) + 1));

// This is an internal class for Document. See FieldIterator for the public version.
class DocumentStorageIterator {
public:
    // DocumentStorage::iterator() and iteratorAll() are easier to use
    DocumentStorageIterator(const ValueElement* first, const ValueElement* end, bool includeMissing)
        : _first(first), _it(first), _end(end), _includeMissing(includeMissing) {
        if (!_includeMissing)
            skipMissing();
    }

    bool atEnd() const {
        return _it == _end;
    }

    const ValueElement& get() const {
        return *_it;
    }

    Position position() const {
        return Position(_it->ptr() - _first->ptr());
    }

    void advance() {
        advanceOne();
        if (!_includeMissing)
            skipMissing();
    }

    const ValueElement* operator->() {
        return _it;
    }
    const ValueElement& operator*() {
        return *_it;
    }

private:
    void advanceOne() {
        _it = _it->next();
    }

    void skipMissing() {
        while (!atEnd() && _it->val.missing()) {
            advanceOne();
        }
    }

    const ValueElement* _first;
    const ValueElement* _it;
    const ValueElement* _end;
    bool _includeMissing;
};

/// Storage class used by both Document and MutableDocument
class DocumentStorage : public RefCountable {
public:
    DocumentStorage()
        : _buffer(nullptr),
          _bufferEnd(nullptr),
          _usedBytes(0),
          _numFields(0),
          _hashTabMask(0),
          _metaFields(),
          _textScore(0),
          _randVal(0),
          _geoNearDistance(0),
          _searchScore(0) {}

    ~DocumentStorage();

    enum MetaType : char {
        TEXT_SCORE,
        RAND_VAL,
        SORT_KEY,
        GEONEAR_DIST,
        GEONEAR_POINT,
        SEARCH_SCORE,
        SEARCH_HIGHLIGHTS,

        // New fields must be added before the NUM_FIELDS sentinel.
        NUM_FIELDS
    };

    static const DocumentStorage& emptyDoc() {
        return kEmptyDoc;
    }

    size_t size() const {
        // can't use _numFields because it includes removed Fields
        size_t count = 0;
        for (DocumentStorageIterator it = iterator(); !it.atEnd(); it.advance())
            count++;
        return count;
    }

    /// Returns the position of the next field to be inserted
    Position getNextPosition() const {
        return Position(_usedBytes);
    }

    /// Returns the position of the named field (may be missing) or Position()
    Position findField(StringData name) const;

    // Document uses these
    const ValueElement& getField(Position pos) const {
        verify(pos.found());
        return *(_firstElement->plusBytes(pos.index));
    }
    Value getField(StringData name) const {
        Position pos = findField(name);
        if (!pos.found())
            return Value();
        return getField(pos).val;
    }

    // MutableDocument uses these
    ValueElement& getField(Position pos) {
        verify(pos.found());
        return *(_firstElement->plusBytes(pos.index));
    }
    Value& getField(StringData name) {
        Position pos = findField(name);
        if (!pos.found())
            return appendField(name);  // TODO: find a way to avoid hashing name twice
        return getField(pos).val;
    }

    /// Adds a new field with missing Value at the end of the document
    Value& appendField(StringData name);

    /** Preallocates space for fields. Use this to attempt to prevent buffer growth.
     *  This is only valid to call before anything is added to the document.
     */
    void reserveFields(size_t expectedFields);

    /// This skips missing values
    DocumentStorageIterator iterator() const {
        return DocumentStorageIterator(_firstElement, end(), false);
    }

    /// This includes missing values
    DocumentStorageIterator iteratorAll() const {
        return DocumentStorageIterator(_firstElement, end(), true);
    }

    /// Shallow copy of this. Caller owns memory.
    boost::intrusive_ptr<DocumentStorage> clone() const;

    size_t allocatedBytes() const {
        return !_buffer ? 0 : (_bufferEnd - _buffer + hashTabBytes());
    }

    /**
     * Compute the space allocated for the metadata fields. Will account for space allocated for
     * unused metadata fields as well.
     */
    size_t getMetadataApproximateSize() const;

    /**
     * Copies all metadata from source if it has any.
     * Note: does not clear metadata from this.
     */
    void copyMetaDataFrom(const DocumentStorage& source) {
        if (source.hasTextScore()) {
            setTextScore(source.getTextScore());
        }
        if (source.hasRandMetaField()) {
            setRandMetaField(source.getRandMetaField());
        }
        if (source.hasSortKeyMetaField()) {
            setSortKeyMetaField(source.getSortKeyMetaField());
        }
        if (source.hasGeoNearDistance()) {
            setGeoNearDistance(source.getGeoNearDistance());
        }
        if (source.hasGeoNearPoint()) {
            setGeoNearPoint(source.getGeoNearPoint());
        }
        if (source.hasSearchScore()) {
            setSearchScore(source.getSearchScore());
        }
        if (source.hasSearchHighlights()) {
            setSearchHighlights(source.getSearchHighlights());
        }
    }

    bool hasTextScore() const {
        return _metaFields.test(MetaType::TEXT_SCORE);
    }
    double getTextScore() const {
        return _textScore;
    }
    void setTextScore(double score) {
        _metaFields.set(MetaType::TEXT_SCORE);
        _textScore = score;
    }

    bool hasRandMetaField() const {
        return _metaFields.test(MetaType::RAND_VAL);
    }
    double getRandMetaField() const {
        return _randVal;
    }
    void setRandMetaField(double val) {
        _metaFields.set(MetaType::RAND_VAL);
        _randVal = val;
    }

    bool hasSortKeyMetaField() const {
        return _metaFields.test(MetaType::SORT_KEY);
    }
    BSONObj getSortKeyMetaField() const {
        return _sortKey;
    }
    void setSortKeyMetaField(BSONObj sortKey) {
        _metaFields.set(MetaType::SORT_KEY);
        _sortKey = sortKey.getOwned();
    }

    bool hasGeoNearDistance() const {
        return _metaFields.test(MetaType::GEONEAR_DIST);
    }
    double getGeoNearDistance() const {
        return _geoNearDistance;
    }
    void setGeoNearDistance(double dist) {
        _metaFields.set(MetaType::GEONEAR_DIST);
        _geoNearDistance = dist;
    }

    bool hasGeoNearPoint() const {
        return _metaFields.test(MetaType::GEONEAR_POINT);
    }
    Value getGeoNearPoint() const {
        return _geoNearPoint;
    }
    void setGeoNearPoint(Value point) {
        _metaFields.set(MetaType::GEONEAR_POINT);
        _geoNearPoint = std::move(point);
    }

    bool hasSearchScore() const {
        return _metaFields.test(MetaType::SEARCH_SCORE);
    }
    double getSearchScore() const {
        return _searchScore;
    }
    void setSearchScore(double score) {
        _metaFields.set(MetaType::SEARCH_SCORE);
        _searchScore = score;
    }

    bool hasSearchHighlights() const {
        return _metaFields.test(MetaType::SEARCH_HIGHLIGHTS);
    }
    Value getSearchHighlights() const {
        return _searchHighlights;
    }
    void setSearchHighlights(Value highlights) {
        _metaFields.set(MetaType::SEARCH_HIGHLIGHTS);
        _searchHighlights = highlights;
    }

private:
    /// Same as lastElement->next() or firstElement() if empty.
    const ValueElement* end() const {
        return _firstElement ? _firstElement->plusBytes(_usedBytes) : nullptr;
    }

    /// Allocates space in _buffer. Copies existing data if there is any.
    void alloc(unsigned newSize);

    /// Call after adding field to _buffer and increasing _numFields
    void addFieldToHashTable(Position pos);

    // assumes _hashTabMask is (power of two) - 1
    unsigned hashTabBuckets() const {
        return _hashTabMask + 1;
    }
    unsigned hashTabBytes() const {
        return hashTabBuckets() * sizeof(Position);
    }

    /// rehash on buffer growth if load-factor > .5 (attempt to keep lf < 1 when full)
    bool needRehash() const {
        return _numFields * 2 > hashTabBuckets();
    }

    /// Initialize empty hash table
    void hashTabInit() {
        memset(static_cast<void*>(_hashTab), -1, hashTabBytes());
    }

    static unsigned hashKey(StringData name) {
        // TODO consider FNV-1a once we have a better benchmark corpus
        unsigned out;
        MurmurHash3_x86_32(name.rawData(), name.size(), 0, &out);
        return out;
    }

    unsigned bucketForKey(StringData name) const {
        return hashKey(name) & _hashTabMask;
    }

    /// Adds all fields to the hash table
    void rehash() {
        hashTabInit();
        for (DocumentStorageIterator it = iteratorAll(); !it.atEnd(); it.advance())
            addFieldToHashTable(it.position());
    }

    enum {
        HASH_TAB_INIT_SIZE = 8,  // must be power of 2
        HASH_TAB_MIN = 4,        // don't hash fields for docs smaller than this
                                 // set to 1 to always hash
    };

    // _buffer layout:
    // -------------------------------------------------------------------------------
    // | ValueElement1 Name1 | ValueElement2 Name2 | ... FREE SPACE ... | Hash Table |
    // -------------------------------------------------------------------------------
    //  ^ _buffer and _firstElement point here                           ^
    //                                _bufferEnd and _hashTab point here ^
    //
    //
    // When the buffer grows, the hash table moves to the new end.
    union {
        char* _buffer;
        ValueElement* _firstElement;
    };

    union {
        // pointer to "end" of _buffer element space and start of hash table (same position)
        char* _bufferEnd;
        Position* _hashTab;  // table lazily initialized once _numFields == HASH_TAB_MIN
    };

    unsigned _usedBytes;    // position where next field would start
    unsigned _numFields;    // this includes removed fields
    unsigned _hashTabMask;  // equal to hashTabBuckets()-1 but used more often

    std::bitset<MetaType::NUM_FIELDS> _metaFields;
    double _textScore;
    double _randVal;
    BSONObj _sortKey;
    double _geoNearDistance;
    Value _geoNearPoint;
    double _searchScore;
    Value _searchHighlights;
    // When adding a field, make sure to update clone() and getMetadataApproximateSize() methods.

    // Defined in document.cpp
    static const DocumentStorage kEmptyDoc;
};
}
