// Retail DB2 (WDC5) reader. Derived from WarcraftXL wxl-db2 (GPL-3.0-or-later),
// https://github.com/WarcraftXL/wxl-db2 -- Copyright (C) 2026 WarcraftXL. Adapted for wowdecomp.
#ifndef ENGINE_SOURCE_RETAIL_DB2_H
#define ENGINE_SOURCE_RETAIL_DB2_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <set>
#include <unordered_map>
#include <vector>

namespace Retail::Db2 {
// Minimal C++17 stand-in for std::span over a constexpr array.
template <class T> struct Span {
    const T* first = nullptr;
    size_t count = 0;
    constexpr Span() = default;
    template <size_t N> constexpr Span(const T (&array)[N]) : first(array), count(N) {}
    constexpr const T* begin() const { return first; }
    constexpr const T* end() const { return first + count; }
    constexpr size_t size() const { return count; }
    constexpr bool empty() const { return !count; }
    constexpr const T& operator[](size_t index) const { return first[index]; }
};
}

namespace Retail::Db2::wdc5
{
    struct FieldShape
    {
        uint16_t elements = 1;
        bool text = false; // inline string: the value is an offset from the field to a NUL-terminated string
    };

    struct Row
    {
        uint32_t id = 0;
        uint32_t parentId = 0;
        // Physical fields are flattened to one allocation. WDC5 item tables contain hundreds of
        // thousands of rows; a vector per field would add millions of client-heap allocations.
        std::vector<uint32_t> values;
        std::vector<std::string> text; // one entry per text field, in field order
    };

    class Table
    {
    public:
        bool Load(const void* bytes, size_t size, const std::vector<FieldShape>& shapes,
                  uint32_t expectedLayoutHash, std::string* error = nullptr);

        const Row* Find(uint32_t id) const noexcept;
        // Appends a copy of row `sourceId` under `id`, with the given fields (element 0) replaced. Invalidates Row pointers.
        bool Clone(uint32_t sourceId, uint32_t id, const std::vector<std::pair<size_t, uint32_t>>& sets);
        uint32_t Value(const Row& row, size_t field, size_t element = 0) const noexcept;
        size_t ElementCount(size_t field) const noexcept;
        // Text of the n-th text field of a row ("" when the table declared none).
        const std::string& Text(const Row& row, size_t textIndex) const noexcept;
        const std::vector<Row>& Rows() const noexcept { return rows_; }
        uint32_t LayoutHash() const noexcept { return layoutHash_; }
        uint32_t TableHash() const noexcept { return tableHash_; }
        const std::string& Schema() const noexcept { return schema_; }

    private:
        uint32_t layoutHash_ = 0;
        uint32_t tableHash_ = 0;
        std::string schema_;
        std::vector<Row> rows_;
        std::unordered_map<uint32_t, size_t> byId_;
        std::vector<size_t> fieldOffsets_;
    };
}

namespace Retail::Db2
{

    /** A physical WDC field in file order. Arrays use elements > 1. */
    struct Field
    {
        std::string_view name;
        uint16_t elements = 1;
        bool text = false;
    };

    /** Where a relationship obtains its source key. */
    enum class RelationSource : uint8_t
    {
        Field,
        RowId,
        ParentId,
    };

    /**
     * Describes a DB2 relationship without forcing either table to remain resident.  ParentId is used for
     * WDC relationship-data columns (DBD annotations `noninline,relation`).
     */
    struct Relation
    {
        std::string_view name;
        RelationSource source = RelationSource::Field;
        std::string_view sourceField;
        uint16_t sourceElement = 0;
        std::string_view targetTable;
        std::string_view targetField = "@id";
    };

    /** Immutable declaration a script supplies for one supported DB2 layout. */
    struct Definition
    {
        std::string_view name;
        std::string_view filename;
        uint32_t layoutHash = 0;
        Span<const Field> fields;
        Span<const Relation> relations{};
        bool required = true;
    };

    /**
     * One decoded DB2 table.  Values can be addressed by their DBD field names instead of fragile numeric
     * positions.  The object owns decoded rows but only borrows the static Definition during Load().
     */
    class Table
    {
    public:
        bool Load(const Definition& definition, std::string* error = nullptr);

        const wdc5::Row* Find(uint32_t id) const noexcept { return table_.Find(id); }
        const std::vector<wdc5::Row>& Rows() const noexcept { return table_.Rows(); }
        // Synthetic row: a copy of `sourceId` under `id` with named fields replaced (the ID field follows `id`).
        bool Clone(uint32_t sourceId, uint32_t id, std::vector<std::pair<std::string_view, uint32_t>> sets);

        uint32_t Value(const wdc5::Row& row, size_t field, size_t element = 0) const noexcept;
        uint32_t Value(const wdc5::Row& row, std::string_view field, size_t element = 0) const noexcept;
        size_t FieldIndex(std::string_view field) const noexcept;
        std::vector<std::string> FieldNames() const;
        // Text of a named string field ("" if the field is not a text field).
        const std::string& Text(const wdc5::Row& row, std::string_view field) const noexcept;
        size_t ElementCount(std::string_view field) const noexcept;
        uint32_t RelationKey(const wdc5::Row& row, std::string_view relation) const noexcept;

        std::string_view Name() const noexcept { return name_; }
        std::string_view Filename() const noexcept { return filename_; }
        uint32_t LayoutHash() const noexcept { return table_.LayoutHash(); }
        uint32_t TableHash() const noexcept { return table_.TableHash(); }
        const std::string& EmbeddedSchema() const noexcept { return table_.Schema(); }

    private:
        struct RelationBinding
        {
            RelationSource source = RelationSource::Field;
            size_t field = static_cast<size_t>(-1);
            uint16_t element = 0;
        };

        std::string name_;
        std::string filename_;
        wdc5::Table table_;
        std::unordered_map<std::string, size_t> fields_;
        std::set<size_t> textFields_;
        std::unordered_map<std::string, RelationBinding> relations_;
    };

    /** Validates names and declared links for a related set before any large DB2 is decoded. */
    bool ValidateDefinitions(Span<const Definition> definitions, std::string* error = nullptr);
}

#endif
