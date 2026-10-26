// Retail DB2 (WDC5) reader. Derived from WarcraftXL wxl-db2 (GPL-3.0-or-later),
// https://github.com/WarcraftXL/wxl-db2 -- Copyright (C) 2026 WarcraftXL. Adapted for wowdecomp.
#include "Engine/Source/Retail/Db2.h"
#include "Engine/Source/Retail/CascSource.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace Retail::Db2::wdc5
{
    namespace
    {
        constexpr uint32_t kMagic = 0x35434457u; // WDC5
        constexpr uint16_t kFlagSparse = 0x0001u;
        constexpr uint16_t kFlagSecondaryKey = 0x0002u;
        constexpr uint16_t kFlagNonInlineId = 0x0004u;

        enum class Compression : uint32_t
        {
            None = 0,
            Immediate = 1,
            Common = 2,
            Pallet = 3,
            PalletArray = 4,
            SignedImmediate = 5,
        };

#pragma pack(push, 1)
        struct Header
        {
            uint32_t magic;
            uint32_t schemaVersion;
            char schema[128];
            uint32_t recordCount;
            uint32_t fieldCount;
            uint32_t recordSize;
            uint32_t stringTableSize;
            uint32_t tableHash;
            uint32_t layoutHash;
            int32_t minId;
            int32_t maxId;
            uint32_t locale;
            uint16_t flags;
            uint16_t idIndex;
            uint32_t totalFieldCount;
            uint32_t bitpackedDataOffset;
            uint32_t lookupColumnCount;
            uint32_t fieldStorageInfoSize;
            uint32_t commonDataSize;
            uint32_t palletDataSize;
            uint32_t sectionCount;
        };

        struct SectionHeader
        {
            uint64_t tactKeyLookup;
            uint32_t fileOffset;
            uint32_t recordCount;
            uint32_t stringTableSize;
            uint32_t offsetRecordsEnd;
            uint32_t idListSize;
            uint32_t relationshipDataSize;
            uint32_t offsetMapIdCount;
            uint32_t copyTableCount;
        };

        struct FieldMeta
        {
            int16_t bits;
            uint16_t offset;
        };

        struct ColumnMeta
        {
            uint16_t recordOffsetBits;
            uint16_t sizeBits;
            uint32_t additionalDataSize;
            uint32_t compression;
            uint32_t value1;
            uint32_t value2;
            uint32_t value3;
        };

        struct SparseEntry
        {
            uint32_t offset;
            uint16_t size;
        };

        struct ReferenceEntry
        {
            uint32_t id;
            uint32_t index;
        };
#pragma pack(pop)

        // WDC5's fixed prefix is 204 bytes. Some readers use 200 as their minimum-size guard,
        // but section_count itself occupies bytes 200..203.
        static_assert(sizeof(Header) == 204);
        static_assert(sizeof(SectionHeader) == 40);
        static_assert(sizeof(FieldMeta) == 4);
        static_assert(sizeof(ColumnMeta) == 24);
        static_assert(sizeof(SparseEntry) == 6);
        static_assert(sizeof(ReferenceEntry) == 8);

        class Cursor
        {
        public:
            Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}

            template <class T>
            bool Read(T& value)
            {
                static_assert(std::is_trivially_copyable_v<T>);
                if (pos_ > size_ || sizeof(T) > size_ - pos_) return false;
                std::memcpy(&value, data_ + pos_, sizeof(T));
                pos_ += sizeof(T);
                return true;
            }

            template <class T>
            bool ReadArray(std::vector<T>& out, size_t count)
            {
                if (count > (std::numeric_limits<size_t>::max)() / sizeof(T)) return false;
                const size_t bytes = count * sizeof(T);
                if (pos_ > size_ || bytes > size_ - pos_) return false;
                out.resize(count);
                if (bytes) std::memcpy(out.data(), data_ + pos_, bytes);
                pos_ += bytes;
                return true;
            }

            bool Seek(size_t pos)
            {
                if (pos > size_) return false;
                pos_ = pos;
                return true;
            }

            bool Skip(size_t bytes)
            {
                return bytes <= size_ - std::min(pos_, size_) && Seek(pos_ + bytes);
            }

            size_t Position() const noexcept { return pos_; }
            const uint8_t* Data() const noexcept { return data_; }
            size_t Size() const noexcept { return size_; }

        private:
            const uint8_t* data_ = nullptr;
            size_t size_ = 0;
            size_t pos_ = 0;
        };

        class BitReader
        {
        public:
            BitReader(const uint8_t* data, size_t bytes) : data_(data), bits_(bytes * 8) {}

            void SetBit(size_t bit) noexcept { bit_ = bit; }
            size_t Bit() const noexcept { return bit_; }

            bool Read(uint32_t count, uint64_t& out)
            {
                if (count > 64 || bit_ > bits_ || count > bits_ - bit_) return false;
                out = 0;
                for (uint32_t i = 0; i < count; ++i)
                {
                    const size_t source = bit_ + i;
                    out |= static_cast<uint64_t>((data_[source >> 3] >> (source & 7)) & 1u) << i;
                }
                bit_ += count;
                return true;
            }

        private:
            const uint8_t* data_ = nullptr;
            size_t bits_ = 0;
            size_t bit_ = 0;
        };

        using CommonMap = std::unordered_map<uint32_t, uint32_t>;

        bool Fail(std::string* error, const char* message)
        {
            if (error) *error = message;
            return false;
        }

        bool DecodeScalar(BitReader& bits, uint32_t rowId, const FieldMeta& field,
                          const ColumnMeta& column, const std::vector<uint32_t>& pallet,
                          const CommonMap& common, uint32_t& out)
        {
            uint64_t value = 0;
            const auto compression = static_cast<Compression>(column.compression);
            switch (compression)
            {
                case Compression::None:
                {
                    int width = 32 - field.bits;
                    if (width <= 0) width = static_cast<int>(column.value2);
                    if (width <= 0 || width > 32 || !bits.Read(static_cast<uint32_t>(width), value)) return false;
                    out = static_cast<uint32_t>(value);
                    return true;
                }
                case Compression::Immediate:
                    if (column.value2 > 32 || !bits.Read(column.value2, value)) return false;
                    out = static_cast<uint32_t>(value);
                    return true;
                case Compression::SignedImmediate:
                {
                    const uint32_t width = column.value2;
                    if (width == 0 || width > 32 || !bits.Read(width, value)) return false;
                    if (width < 32 && (value & (uint64_t{1} << (width - 1))))
                        value |= ~((uint64_t{1} << width) - 1);
                    out = static_cast<uint32_t>(value);
                    return true;
                }
                case Compression::Common:
                {
                    const auto it = common.find(rowId);
                    out = it == common.end() ? column.value1 : it->second;
                    return true;
                }
                case Compression::Pallet:
                case Compression::PalletArray:
                    if (column.value2 > 32 || !bits.Read(column.value2, value) || value >= pallet.size()) return false;
                    out = pallet[static_cast<size_t>(value)];
                    return true;
            }
            return false;
        }
    }

    bool Table::Load(const void* bytes, size_t size, const std::vector<FieldShape>& shapes,
                     uint32_t expectedLayoutHash, std::string* error)
    {
        rows_.clear();
        byId_.clear();
        fieldOffsets_.clear();
        layoutHash_ = 0;
        tableHash_ = 0;
        schema_.clear();
        if (error) error->clear();
        if (!bytes || size < sizeof(Header)) return Fail(error, "WDC5: file is smaller than its header");

        Cursor cursor(static_cast<const uint8_t*>(bytes), size);
        Header header{};
        if (!cursor.Read(header) || header.magic != kMagic) return Fail(error, "WDC5: invalid magic");
        if (expectedLayoutHash && header.layoutHash != expectedLayoutHash)
            return Fail(error, "WDC5: layout hash does not match the compiled schema");
        if (header.fieldCount == 0 || shapes.size() != header.fieldCount)
            return Fail(error, "WDC5: physical field count does not match the compiled schema");
        if (header.sectionCount == 0 && header.recordCount != 0)
            return Fail(error, "WDC5: records exist but the section table is empty");
        if ((header.flags & kFlagSparse) != 0)
            return Fail(error, "WDC5: sparse tables are not supported by this compatibility layer");

        fieldOffsets_.resize(header.fieldCount + 1);
        for (size_t i = 0; i < header.fieldCount; ++i)
            fieldOffsets_[i + 1] = fieldOffsets_[i] + std::max<uint16_t>(1, shapes[i].elements);

        layoutHash_ = header.layoutHash;
        tableHash_ = header.tableHash;
        schema_.assign(header.schema, header.schema + sizeof(header.schema));
        schema_.erase(std::find(schema_.begin(), schema_.end(), '\0'), schema_.end());

        std::vector<SectionHeader> sections;
        std::vector<FieldMeta> fieldMeta;
        std::vector<ColumnMeta> columnMeta;
        if (!cursor.ReadArray(sections, header.sectionCount) ||
            !cursor.ReadArray(fieldMeta, header.fieldCount))
            return Fail(error, "WDC5: truncated metadata");
        // Forever ships truly empty tables with field descriptors and no column storage.
        // Require the exact empty layout; truncated nonempty tables must still fail.
        if (!header.recordCount && !header.sectionCount && !header.fieldStorageInfoSize &&
            !header.commonDataSize && !header.palletDataSize && !header.stringTableSize &&
            size == sizeof(Header) + size_t(header.fieldCount) * sizeof(FieldMeta)) return true;
        if (!cursor.ReadArray(columnMeta, header.fieldCount)) return Fail(error, "WDC5: truncated metadata");

        std::vector<std::vector<uint32_t>> pallets(header.fieldCount);
        std::vector<CommonMap> commons(header.fieldCount);
        for (size_t i = 0; i < columnMeta.size(); ++i)
        {
            const auto compression = static_cast<Compression>(columnMeta[i].compression);
            if (compression == Compression::Pallet || compression == Compression::PalletArray)
            {
                if ((columnMeta[i].additionalDataSize & 3u) != 0 ||
                    !cursor.ReadArray(pallets[i], columnMeta[i].additionalDataSize / 4u))
                    return Fail(error, "WDC5: truncated pallet data");
            }
        }
        for (size_t i = 0; i < columnMeta.size(); ++i)
        {
            if (static_cast<Compression>(columnMeta[i].compression) != Compression::Common) continue;
            if ((columnMeta[i].additionalDataSize & 7u) != 0)
                return Fail(error, "WDC5: malformed common-data size");
            const size_t count = columnMeta[i].additionalDataSize / 8u;
            for (size_t j = 0; j < count; ++j)
            {
                uint32_t id = 0, value = 0;
                if (!cursor.Read(id) || !cursor.Read(value)) return Fail(error, "WDC5: truncated common data");
                commons[i][id] = value;
            }
        }

        // Encrypted-id lists are stored between column auxiliary data and the first section.
        for (const SectionHeader& section : sections)
        {
            if (section.tactKeyLookup == 0) continue;
            uint32_t count = 0;
            if (!cursor.Read(count) || !cursor.Skip(static_cast<size_t>(count) * sizeof(uint32_t)))
                return Fail(error, "WDC5: truncated encrypted-id list");
        }

        rows_.reserve(header.recordCount);
        std::unordered_map<uint32_t, uint32_t> copies;
        uint32_t globalRecordIndex = 0;
        for (const SectionHeader& section : sections)
        {
            if (!cursor.Seek(section.fileOffset)) return Fail(error, "WDC5: section offset is outside the file");
            const size_t recordBytes = static_cast<size_t>(section.recordCount) * header.recordSize;
            if (header.recordSize && recordBytes / header.recordSize != section.recordCount)
                return Fail(error, "WDC5: record byte count overflow");
            if (cursor.Position() > size || recordBytes > size - cursor.Position())
                return Fail(error, "WDC5: truncated record block");
            const uint8_t* recordData = cursor.Data() + cursor.Position();
            // A section encrypted for a key we lack arrives zero-filled; stock clients skip it too.
            const bool unreadable = section.tactKeyLookup != 0 &&
                std::all_of(recordData, recordData + recordBytes, [](uint8_t byte) { return byte == 0; });
            if (!cursor.Skip(recordBytes) || !cursor.Skip(section.stringTableSize))
                return Fail(error, "WDC5: truncated record/string block");

            std::vector<uint32_t> ids;
            if ((section.idListSize & 3u) != 0 || !cursor.ReadArray(ids, section.idListSize / 4u))
                return Fail(error, "WDC5: truncated id list");
            if (!ids.empty() && ids.size() != section.recordCount)
                return Fail(error, "WDC5: id-list length does not match section records");

            for (uint32_t i = 0; i < section.copyTableCount; ++i)
            {
                uint32_t destination = 0, source = 0;
                if (!cursor.Read(destination) || !cursor.Read(source))
                    return Fail(error, "WDC5: truncated copy table");
                if (destination != source) copies[destination] = source;
            }

            if (section.offsetMapIdCount != 0)
                return Fail(error, "WDC5: unexpected offset map in a non-sparse retail table");

            std::unordered_map<uint32_t, uint32_t> parentByIndex;
            if (section.relationshipDataSize)
            {
                const size_t relationshipStart = cursor.Position();
                uint32_t count = 0, minId = 0, maxId = 0;
                if (!cursor.Read(count) || !cursor.Read(minId) || !cursor.Read(maxId))
                    return Fail(error, "WDC5: truncated relationship header");
                (void)minId;
                (void)maxId;
                for (uint32_t i = 0; i < count; ++i)
                {
                    ReferenceEntry entry{};
                    if (!cursor.Read(entry)) return Fail(error, "WDC5: truncated relationship data");
                    parentByIndex[entry.index] = entry.id;
                }
                const size_t consumed = cursor.Position() - relationshipStart;
                if (consumed > section.relationshipDataSize ||
                    !cursor.Skip(section.relationshipDataSize - consumed))
                    return Fail(error, "WDC5: malformed relationship-data size");
            }

            if (unreadable) { globalRecordIndex += section.recordCount; continue; }

            for (uint32_t i = 0; i < section.recordCount; ++i, ++globalRecordIndex)
            {
                Row row;
                row.values.resize(fieldOffsets_.back());
                BitReader bits(recordData + static_cast<size_t>(i) * header.recordSize, header.recordSize);

                if ((header.flags & kFlagNonInlineId) != 0)
                {
                    if (ids.empty()) return Fail(error, "WDC5: non-inline ID table has no id list");
                    row.id = ids[i];
                }

                for (size_t fieldIndex = 0; fieldIndex < header.fieldCount; ++fieldIndex)
                {
                    const ColumnMeta& column = columnMeta[fieldIndex];
                    const auto compression = static_cast<Compression>(column.compression);
                    uint16_t elements = std::max<uint16_t>(1, shapes[fieldIndex].elements);
                    if (compression == Compression::PalletArray)
                        elements = static_cast<uint16_t>(column.value3);

                    // The compiled shape and WDC5 pallet cardinality should describe the same logical array.
                    if (fieldOffsets_[fieldIndex + 1] - fieldOffsets_[fieldIndex] != elements)
                        return Fail(error, "WDC5: pallet cardinality does not match the compiled schema");
                    uint32_t* values = row.values.data() + fieldOffsets_[fieldIndex];
                    if (compression == Compression::PalletArray)
                    {
                        uint64_t palletIndex = 0;
                        if (column.value2 > 32 || !bits.Read(column.value2, palletIndex))
                            return Fail(error, "WDC5: invalid pallet-array index");
                        const size_t first = static_cast<size_t>(palletIndex) * elements;
                        if (first > pallets[fieldIndex].size() || elements > pallets[fieldIndex].size() - first)
                            return Fail(error, "WDC5: pallet-array index is outside its pallet");
                        std::copy_n(pallets[fieldIndex].begin() + first, elements, values);
                    }
                    else
                    {
                        for (uint16_t element = 0; element < elements; ++element)
                        {
                            if (!DecodeScalar(bits, row.id, fieldMeta[fieldIndex], column,
                                              pallets[fieldIndex], commons[fieldIndex], values[element]))
                                return Fail(error, "WDC5: failed to decode a field");
                        }
                    }

                    if (shapes[fieldIndex].text) {
                        // Inline strings are stored as an offset from the field's own position in the file.
                        const size_t at = size_t(section.fileOffset) + size_t(i) * header.recordSize + fieldMeta[fieldIndex].offset + values[0];
                        std::string text;
                        for (size_t p = at; p < size && cursor.Data()[p]; ++p) text.push_back(char(cursor.Data()[p]));
                        row.text.push_back(std::move(text));
                    }

                    if ((header.flags & kFlagNonInlineId) == 0 && fieldIndex == header.idIndex)
                        row.id = elements == 0 ? 0 : values[0];
                }

                const uint32_t relationshipKey = (header.flags & kFlagSecondaryKey) != 0 ? row.id : i;
                const auto parent = parentByIndex.find(relationshipKey);
                if (parent != parentByIndex.end()) row.parentId = parent->second;
                rows_.push_back(std::move(row));
            }
        }

        for (const auto& [destination, source] : copies)
        {
            const auto it = std::find_if(rows_.begin(), rows_.end(), [source](const Row& row) { return row.id == source; });
            if (it == rows_.end()) continue;
            Row copy = *it;
            copy.id = destination;
            rows_.push_back(std::move(copy));
        }

        byId_.reserve(rows_.size());
        for (size_t i = 0; i < rows_.size(); ++i) byId_[rows_[i].id] = i;
        return true;
    }

    const Row* Table::Find(uint32_t id) const noexcept
    {
        const auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : &rows_[it->second];
    }

    bool Table::Clone(uint32_t sourceId, uint32_t id, const std::vector<std::pair<size_t, uint32_t>>& sets)
    {
        const Row* source = Find(sourceId);
        if (!source || Find(id)) return false;
        Row copy = *source;
        copy.id = id;
        for (const auto& [field, value] : sets)
        {
            if (field + 1 >= fieldOffsets_.size() || fieldOffsets_[field] >= copy.values.size()) return false;
            copy.values[fieldOffsets_[field]] = value;
        }
        rows_.push_back(std::move(copy));
        byId_[id] = rows_.size() - 1;
        return true;
    }

    uint32_t Table::Value(const Row& row, size_t field, size_t element) const noexcept
    {
        if (field + 1 >= fieldOffsets_.size()) return 0;
        const size_t begin = fieldOffsets_[field];
        const size_t end = fieldOffsets_[field + 1];
        return element < end - begin && begin + element < row.values.size() ? row.values[begin + element] : 0;
    }

    const std::string& Table::Text(const Row& row, size_t textIndex) const noexcept
    {
        static const std::string empty;
        return textIndex < row.text.size() ? row.text[textIndex] : empty;
    }

    size_t Table::ElementCount(size_t field) const noexcept
    {
        return field + 1 < fieldOffsets_.size() ? fieldOffsets_[field + 1] - fieldOffsets_[field] : 0;
    }
}

namespace Retail::Db2
{
    namespace
    {
        constexpr size_t kMissingField = static_cast<size_t>(-1);

        bool Fail(std::string* error, std::string message)
        {
            if (error) *error = std::move(message);
            return false;
        }

        // FileDataIDs of the DBFilesClient tables, from the community listfile. Retail CASC
        // roots carry no names for these, so a table is fetched by ID.
        uint32_t TableFileDataId(std::string_view filename)
        {
            static const struct { const char* stem; uint32_t id; } kTables[] = {
                {"charcomponenttexturelayouts", 1360262}, {"charcomponenttexturesections", 1360263},
                {"chrcustomizationchoice", 3450554}, {"chrcustomizationelement", 3512765},
                {"chrcustomizationcondmodel", 3460182},
                {"chrcustomizationgeoset", 3456171}, {"chrcustomizationmaterial", 3459652},
                {"chrcustomizationoption", 3384247}, {"chrcustomizationskinnedmodel", 3460183},
                {"chrmodel", 3384313}, {"chrmodeltexturelayer", 3548976}, {"chrraces", 1305311},
                {"chrracexchrmodel", 3490304}, {"creaturedisplayinfo", 1108759},
                {"creaturedisplayinfogeosetdata", 1720141}, {"creaturemodeldata", 1365368},
                {"modelfiledata", 1337833}, {"texturefiledata", 982459},
            };
            std::string stem(filename);
            std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            const size_t slash = stem.find_last_of("/\\");
            if (slash != std::string::npos) stem.erase(0, slash + 1);
            const size_t dot = stem.rfind(".db2");
            if (dot != std::string::npos) stem.erase(dot);
            for (const auto& table : kTables)
                if (stem == table.stem) return table.id;
            return 0;
        }

        bool ReadWholeFile(std::string_view filename, std::vector<uint8_t>& bytes, std::string& error)
        {
            bytes.clear();
            const uint32_t id = TableFileDataId(filename);
            if (!id) { error = "no FileDataID known for " + std::string(filename); return false; }
            return Source().ReadTable((std::string(SourcePrefix()) + std::to_string(id) + ".db2").c_str(), bytes, error);
        }

        const Definition* FindDefinition(Span<const Definition> definitions, std::string_view name)
        {
            for (const Definition& definition : definitions)
                if (definition.name == name) return &definition;
            return nullptr;
        }

        bool HasField(const Definition& definition, std::string_view name)
        {
            if (name == "@id" || name == "@parent") return true;
            return std::any_of(definition.fields.begin(), definition.fields.end(),
                [name](const Field& field) { return field.name == name; });
        }
    }

    bool Table::Load(const Definition& definition, std::string* error)
    {
        name_.clear();
        filename_.clear();
        fields_.clear();
        relations_.clear();
        textFields_.clear();
        table_ = {};
        if (error) error->clear();
        if (definition.name.empty() || definition.filename.empty())
            return Fail(error, "DB2: table name and filename are required");
        if (definition.fields.empty())
            return Fail(error, "DB2: " + std::string(definition.name) + " has no physical fields");

        std::vector<wdc5::FieldShape> shapes;
        shapes.reserve(definition.fields.size());
        fields_.reserve(definition.fields.size());
        for (size_t i = 0; i < definition.fields.size(); ++i)
        {
            const Field& field = definition.fields[i];
            if (field.name.empty() || !fields_.emplace(std::string(field.name), i).second)
                return Fail(error, "DB2: " + std::string(definition.name) + " has an empty or duplicate field name");
            shapes.push_back({std::max<uint16_t>(1, field.elements), field.text});
            if (field.text) textFields_.insert(i);
        }

        for (const Relation& relation : definition.relations)
        {
            RelationBinding binding{relation.source, kMissingField, relation.sourceElement};
            if (relation.source == RelationSource::Field)
            {
                const auto field = fields_.find(std::string(relation.sourceField));
                if (field == fields_.end())
                    return Fail(error, "DB2: relation " + std::string(relation.name) + " names an unknown source field");
                binding.field = field->second;
                if (binding.element >= std::max<uint16_t>(1, definition.fields[binding.field].elements))
                    return Fail(error, "DB2: relation " + std::string(relation.name) + " has an invalid array element");
            }
            if (relation.name.empty() || !relations_.emplace(std::string(relation.name), binding).second)
                return Fail(error, "DB2: " + std::string(definition.name) + " has an empty or duplicate relation name");
        }

        std::vector<uint8_t> bytes;
        std::string readError;
        if (!ReadWholeFile(definition.filename, bytes, readError))
            return Fail(error, "DB2: unable to read " + std::string(definition.filename) + ": " + readError);
        if (!table_.Load(bytes.data(), bytes.size(), shapes, definition.layoutHash, error)) return false;

        name_.assign(definition.name);
        filename_.assign(definition.filename);
        return true;
    }

    uint32_t Table::Value(const wdc5::Row& row, size_t field, size_t element) const noexcept
    {
        return table_.Value(row, field, element);
    }

    uint32_t Table::Value(const wdc5::Row& row, std::string_view field, size_t element) const noexcept
    {
        const size_t index = FieldIndex(field);
        return index == kMissingField ? 0 : table_.Value(row, index, element);
    }

    bool Table::Clone(uint32_t sourceId, uint32_t id, std::vector<std::pair<std::string_view, uint32_t>> sets)
    {
        std::vector<std::pair<size_t, uint32_t>> indexed;
        for (const auto& [name, value] : sets)
        {
            const size_t index = FieldIndex(name);
            if (index == kMissingField) return false;
            indexed.emplace_back(index, value);
        }
        if (const size_t idField = FieldIndex("ID"); idField != kMissingField) indexed.emplace_back(idField, id);
        return table_.Clone(sourceId, id, indexed);
    }

    size_t Table::FieldIndex(std::string_view field) const noexcept
    {
        const auto it = fields_.find(std::string(field));
        return it == fields_.end() ? kMissingField : it->second;
    }

    const std::string& Table::Text(const wdc5::Row& row, std::string_view field) const noexcept
    {
        static const std::string empty;
        size_t index = 0;
        for (const auto& [name, position] : fields_) {
            if (name != field) continue;
            if (!textFields_.count(position)) return empty;
            for (size_t other : textFields_) if (other < position) ++index;
            return table_.Text(row, index);
        }
        return empty;
    }

    std::vector<std::string> Table::FieldNames() const
    {
        std::vector<std::string> names(fields_.size());
        for (const auto& [name, index] : fields_) names[index] = name;
        return names;
    }

    size_t Table::ElementCount(std::string_view field) const noexcept
    {
        const size_t index = FieldIndex(field);
        return index == kMissingField ? 0 : table_.ElementCount(index);
    }

    uint32_t Table::RelationKey(const wdc5::Row& row, std::string_view relation) const noexcept
    {
        const auto it = relations_.find(std::string(relation));
        if (it == relations_.end()) return 0;
        switch (it->second.source)
        {
            case RelationSource::RowId: return row.id;
            case RelationSource::ParentId: return row.parentId;
            case RelationSource::Field: return table_.Value(row, it->second.field, it->second.element);
        }
        return 0;
    }

    bool ValidateDefinitions(Span<const Definition> definitions, std::string* error)
    {
        if (error) error->clear();
        std::unordered_set<std::string> names;
        for (const Definition& definition : definitions)
        {
            if (definition.name.empty() || definition.filename.empty() || definition.fields.empty())
                return Fail(error, "DB2: every definition needs a name, filename, and physical fields");
            if (!names.emplace(definition.name).second)
                return Fail(error, "DB2: duplicate table definition " + std::string(definition.name));
            std::unordered_set<std::string> fields;
            for (const Field& field : definition.fields)
                if (field.name.empty() || !fields.emplace(field.name).second)
                    return Fail(error, "DB2: duplicate or empty field in " + std::string(definition.name));
        }

        for (const Definition& definition : definitions)
        {
            for (const Relation& relation : definition.relations)
            {
                if (relation.name.empty() || relation.targetTable.empty())
                    return Fail(error, "DB2: incomplete relation in " + std::string(definition.name));
                if (relation.source == RelationSource::Field && !HasField(definition, relation.sourceField))
                    return Fail(error, "DB2: relation source field is absent from " + std::string(definition.name));
                const Definition* target = FindDefinition(definitions, relation.targetTable);
                if (!target)
                    return Fail(error, "DB2: relation target table " + std::string(relation.targetTable) + " is not declared");
                if (!HasField(*target, relation.targetField))
                    return Fail(error, "DB2: relation target field is absent from " + std::string(target->name));
            }
        }
        return true;
    }
}
