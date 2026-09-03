#include "acpi_tables.h"

static unsigned int read_u16(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8U);
}

static unsigned int read_u32(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
        ((unsigned int)bytes[1] << 8U) |
        ((unsigned int)bytes[2] << 16U) |
        ((unsigned int)bytes[3] << 24U);
}

static int bytes_equal(const unsigned char *left, const char *right,
    unsigned int length)
{
    unsigned int index;

    for (index = 0U; index < length; index++) {
        if (left[index] != (unsigned char)right[index]) {
            return 0;
        }
    }
    return 1;
}

int acpi_checksum_valid(const unsigned char *bytes, unsigned int length)
{
    unsigned int index;
    unsigned char sum = 0U;

    if (bytes == 0 || length == 0U) {
        return 0;
    }
    for (index = 0U; index < length; index++) {
        sum = (unsigned char)(sum + bytes[index]);
    }
    return sum == 0U;
}

int acpi_parse_rsdp(const unsigned char *bytes, unsigned int available,
    struct acpi_rsdp_info *info)
{
    unsigned int revision;
    unsigned int length = ACPI_RSDP_V1_LENGTH;
    unsigned int rsdt_address;

    if (bytes == 0 || info == 0 || available < ACPI_RSDP_V1_LENGTH ||
        !bytes_equal(bytes, "RSD PTR ", 8U) ||
        !acpi_checksum_valid(bytes, ACPI_RSDP_V1_LENGTH)) {
        return 0;
    }
    revision = bytes[15];
    rsdt_address = read_u32(bytes + 16U);
    if (revision == 1U || rsdt_address < 0x1000U) {
        return 0;
    }
    if (revision >= 2U) {
        length = read_u32(bytes + 20U);
        if (length < ACPI_RSDP_V2_LENGTH || length > ACPI_MAX_RSDP_LENGTH ||
            length > available || !acpi_checksum_valid(bytes, length) ||
            bytes[33] != 0U || bytes[34] != 0U || bytes[35] != 0U) {
            return 0;
        }
    }
    info->rsdt_address = rsdt_address;
    info->xsdt_address_low = revision >= 2U ? read_u32(bytes + 24U) : 0U;
    info->xsdt_address_high = revision >= 2U ? read_u32(bytes + 28U) : 0U;
    info->revision = revision;
    info->length = length;
    return 1;
}

int acpi_sdt_peek_length(const unsigned char *bytes, unsigned int available,
    const char signature[4], unsigned int *length)
{
    unsigned int table_length;

    if (bytes == 0 || signature == 0 || length == 0 ||
        available < ACPI_SDT_HEADER_LENGTH ||
        !bytes_equal(bytes, signature, 4U)) {
        return 0;
    }
    table_length = read_u32(bytes + 4U);
    if (table_length < ACPI_SDT_HEADER_LENGTH ||
        table_length > ACPI_MAX_SDT_LENGTH) {
        return 0;
    }
    *length = table_length;
    return 1;
}

int acpi_parse_sdt(const unsigned char *bytes, unsigned int available,
    const char signature[4], unsigned int *length)
{
    unsigned int table_length;

    if (!acpi_sdt_peek_length(bytes, available, signature, &table_length) ||
        table_length > available || !acpi_checksum_valid(bytes, table_length)) {
        return 0;
    }
    *length = table_length;
    return 1;
}

int acpi_rsdt_entry_at(const unsigned char *bytes, unsigned int available,
    unsigned int index, unsigned int *physical_address,
    unsigned int *entry_count)
{
    unsigned int length;
    unsigned int count;
    unsigned int address;

    if (physical_address == 0 || entry_count == 0 ||
        !acpi_parse_sdt(bytes, available, "RSDT", &length) ||
        ((length - ACPI_SDT_HEADER_LENGTH) & 3U) != 0U) {
        return 0;
    }
    count = (length - ACPI_SDT_HEADER_LENGTH) / 4U;
    if (count == 0U || count > ACPI_MAX_RSDT_ENTRIES || index >= count) {
        return 0;
    }
    address = read_u32(bytes + ACPI_SDT_HEADER_LENGTH + index * 4U);
    if (address < 0x1000U) {
        return 0;
    }
    *physical_address = address;
    *entry_count = count;
    return 1;
}

int acpi_xsdt_entry_at(const unsigned char *bytes, unsigned int available,
    unsigned int index, unsigned int *physical_address_low,
    unsigned int *physical_address_high, unsigned int *entry_count)
{
    unsigned int length;
    unsigned int count;
    unsigned int offset;
    unsigned int low;
    unsigned int high;

    if (physical_address_low == 0 || physical_address_high == 0 ||
        entry_count == 0 ||
        !acpi_parse_sdt(bytes, available, "XSDT", &length) ||
        ((length - ACPI_SDT_HEADER_LENGTH) & 7U) != 0U) {
        return 0;
    }
    count = (length - ACPI_SDT_HEADER_LENGTH) / 8U;
    if (count == 0U || count > ACPI_MAX_RSDT_ENTRIES || index >= count) {
        return 0;
    }
    offset = ACPI_SDT_HEADER_LENGTH + index * 8U;
    low = read_u32(bytes + offset);
    high = read_u32(bytes + offset + 4U);
    if (low < 0x1000U && high == 0U) {
        return 0;
    }
    *physical_address_low = low;
    *physical_address_high = high;
    *entry_count = count;
    return 1;
}

int acpi_parse_hpet_table(const unsigned char *bytes, unsigned int available,
    struct acpi_hpet_table_info *info)
{
    unsigned int length;
    unsigned int block_id;
    unsigned int address_low;
    unsigned int address_high;
    unsigned int vendor;
    unsigned char access_size;
    unsigned char page_protection;

    if (info == 0 || !acpi_parse_sdt(bytes, available, "HPET", &length) ||
        length < ACPI_HPET_TABLE_MIN_LENGTH || bytes[8] == 0U) {
        return 0;
    }
    block_id = read_u32(bytes + 36U);
    vendor = block_id >> 16U;
    access_size = bytes[43];
    address_low = read_u32(bytes + 44U);
    address_high = read_u32(bytes + 48U);
    page_protection = bytes[55];
    if ((block_id & (1U << 14U)) != 0U || vendor == 0U || vendor == 0xFFFFU ||
        bytes[40] != 0U || (bytes[41] != 0U && bytes[41] != 64U) ||
        bytes[42] != 0U ||
        (access_size != 0U && access_size != 4U) || address_high != 0U ||
        address_low < 0x1000U || (address_low & 0x3FFU) != 0U ||
        address_low > 0xFFFFFFFFU - 1023U || page_protection > 2U) {
        return 0;
    }
    info->event_timer_block_id = block_id;
    info->physical_address = address_low;
    info->minimum_tick = read_u16(bytes + 53U);
    info->sequence_number = bytes[52];
    info->page_protection = page_protection;
    return 1;
}

static int madt_interrupt_flags_valid(unsigned int flags)
{
    unsigned int polarity = flags & 3U;
    unsigned int trigger = (flags >> 2U) & 3U;

    return (flags & ~0xFU) == 0U && polarity != 2U && trigger != 2U;
}

static int madt_mmio_address_valid(unsigned int address)
{
    return address >= 0x1000U && (address & 0xFFFU) == 0U &&
        address <= 0xFFFFFFFFU - 0xFFFU;
}

static void madt_info_clear(struct acpi_madt_table_info *info)
{
    unsigned int index;

    info->local_apic_address = 0U;
    info->flags = 0U;
    info->enabled_local_apics = 0U;
    info->enabled_x2apics = 0U;
    info->io_apic_count = 0U;
    for (index = 0U; index < ACPI_MADT_MAX_IO_APICS; index++) {
        info->io_apics[index].id = 0U;
        info->io_apics[index].physical_address = 0U;
        info->io_apics[index].global_interrupt_base = 0U;
    }
    for (index = 0U; index < ACPI_MADT_ISA_IRQ_COUNT; index++) {
        info->isa_overrides[index].global_interrupt = index;
        info->isa_overrides[index].flags = 0U;
        info->isa_overrides[index].present = 0U;
    }
}

static void madt_info_copy(struct acpi_madt_table_info *destination,
    const struct acpi_madt_table_info *source)
{
    unsigned int index;

    destination->local_apic_address = source->local_apic_address;
    destination->flags = source->flags;
    destination->enabled_local_apics = source->enabled_local_apics;
    destination->enabled_x2apics = source->enabled_x2apics;
    destination->io_apic_count = source->io_apic_count;
    for (index = 0U; index < ACPI_MADT_MAX_IO_APICS; index++) {
        destination->io_apics[index].id = source->io_apics[index].id;
        destination->io_apics[index].physical_address =
            source->io_apics[index].physical_address;
        destination->io_apics[index].global_interrupt_base =
            source->io_apics[index].global_interrupt_base;
    }
    for (index = 0U; index < ACPI_MADT_ISA_IRQ_COUNT; index++) {
        destination->isa_overrides[index].global_interrupt =
            source->isa_overrides[index].global_interrupt;
        destination->isa_overrides[index].flags =
            source->isa_overrides[index].flags;
        destination->isa_overrides[index].present =
            source->isa_overrides[index].present;
    }
}

int acpi_parse_madt_table(const unsigned char *bytes, unsigned int available,
    struct acpi_madt_table_info *info)
{
    struct acpi_madt_table_info parsed;
    unsigned char local_apic_ids[256];
    unsigned int x2apic_ids[ACPI_MADT_MAX_PROCESSORS];
    unsigned int x2apic_id_count = 0U;
    unsigned int structure_count = 0U;
    unsigned int length;
    unsigned int offset;
    unsigned int index;
    int address_override_seen = 0;

    if (info == 0 || !acpi_parse_sdt(bytes, available, "APIC", &length) ||
        length < ACPI_MADT_HEADER_LENGTH || bytes[8] == 0U) {
        return 0;
    }
    madt_info_clear(&parsed);
    for (index = 0U; index < 256U; index++) {
        local_apic_ids[index] = 0U;
    }
    parsed.local_apic_address = read_u32(bytes + 36U);
    parsed.flags = read_u32(bytes + 40U);
    if ((parsed.flags & ~1U) != 0U) {
        return 0;
    }

    offset = ACPI_MADT_HEADER_LENGTH;
    while (offset < length) {
        unsigned int type;
        unsigned int structure_length;

        if (length - offset < 2U) {
            return 0;
        }
        type = bytes[offset];
        structure_length = bytes[offset + 1U];
        if (structure_length < 2U || structure_length > length - offset ||
            ++structure_count > 1024U) {
            return 0;
        }
        if (type == 0U) {
            unsigned int apic_id;
            unsigned int flags;

            if (structure_length != 8U) {
                return 0;
            }
            apic_id = bytes[offset + 3U];
            flags = read_u32(bytes + offset + 4U);
            if ((flags & ~3U) != 0U || (flags & 3U) == 3U) {
                return 0;
            }
            if ((flags & 1U) != 0U) {
                if (local_apic_ids[apic_id] != 0U ||
                    parsed.enabled_local_apics + parsed.enabled_x2apics >=
                        ACPI_MADT_MAX_PROCESSORS) {
                    return 0;
                }
                local_apic_ids[apic_id] = 1U;
                parsed.enabled_local_apics++;
            }
        } else if (type == 1U) {
            unsigned int io_index;
            unsigned int id;
            unsigned int address;

            if (structure_length != 12U || bytes[offset + 3U] != 0U ||
                parsed.io_apic_count >= ACPI_MADT_MAX_IO_APICS) {
                return 0;
            }
            id = bytes[offset + 2U];
            address = read_u32(bytes + offset + 4U);
            if (!madt_mmio_address_valid(address)) {
                return 0;
            }
            for (io_index = 0U; io_index < parsed.io_apic_count; io_index++) {
                if (parsed.io_apics[io_index].id == id ||
                    parsed.io_apics[io_index].physical_address == address) {
                    return 0;
                }
            }
            io_index = parsed.io_apic_count++;
            parsed.io_apics[io_index].id = id;
            parsed.io_apics[io_index].physical_address = address;
            parsed.io_apics[io_index].global_interrupt_base =
                read_u32(bytes + offset + 8U);
        } else if (type == 2U) {
            unsigned int source;
            unsigned int flags;

            if (structure_length != 10U || bytes[offset + 2U] != 0U) {
                return 0;
            }
            source = bytes[offset + 3U];
            flags = read_u16(bytes + offset + 8U);
            if (source >= ACPI_MADT_ISA_IRQ_COUNT ||
                !madt_interrupt_flags_valid(flags) ||
                parsed.isa_overrides[source].present != 0U) {
                return 0;
            }
            parsed.isa_overrides[source].global_interrupt =
                read_u32(bytes + offset + 4U);
            parsed.isa_overrides[source].flags = flags;
            parsed.isa_overrides[source].present = 1U;
        } else if (type == 3U) {
            if (structure_length != 8U || !madt_interrupt_flags_valid(
                    read_u16(bytes + offset + 2U))) {
                return 0;
            }
        } else if (type == 4U) {
            if (structure_length != 6U || bytes[offset + 5U] > 1U ||
                !madt_interrupt_flags_valid(read_u16(bytes + offset + 3U))) {
                return 0;
            }
        } else if (type == 5U) {
            unsigned int address_low;
            unsigned int address_high;

            if (structure_length != 12U || address_override_seen ||
                read_u16(bytes + offset + 2U) != 0U) {
                return 0;
            }
            address_low = read_u32(bytes + offset + 4U);
            address_high = read_u32(bytes + offset + 8U);
            if (address_high != 0U || !madt_mmio_address_valid(address_low)) {
                return 0;
            }
            parsed.local_apic_address = address_low;
            address_override_seen = 1;
        } else if (type == 9U) {
            unsigned int apic_id;
            unsigned int flags;

            if (structure_length != 16U ||
                read_u16(bytes + offset + 2U) != 0U) {
                return 0;
            }
            apic_id = read_u32(bytes + offset + 4U);
            flags = read_u32(bytes + offset + 8U);
            if ((flags & ~3U) != 0U || (flags & 3U) == 3U) {
                return 0;
            }
            if ((flags & 1U) != 0U) {
                if (parsed.enabled_local_apics + parsed.enabled_x2apics >=
                    ACPI_MADT_MAX_PROCESSORS) {
                    return 0;
                }
                for (index = 0U; index < x2apic_id_count; index++) {
                    if (x2apic_ids[index] == apic_id) {
                        return 0;
                    }
                }
                x2apic_ids[x2apic_id_count++] = apic_id;
                parsed.enabled_x2apics++;
            }
        } else if (type == 10U) {
            if (structure_length != 12U || bytes[offset + 8U] > 1U ||
                bytes[offset + 9U] != 0U || bytes[offset + 10U] != 0U ||
                bytes[offset + 11U] != 0U ||
                !madt_interrupt_flags_valid(read_u16(bytes + offset + 2U))) {
                return 0;
            }
        }
        offset += structure_length;
    }
    if (parsed.enabled_local_apics + parsed.enabled_x2apics == 0U ||
        !madt_mmio_address_valid(parsed.local_apic_address)) {
        return 0;
    }
    for (index = 0U; index < parsed.io_apic_count; index++) {
        if (parsed.io_apics[index].physical_address ==
            parsed.local_apic_address) {
            return 0;
        }
    }
    madt_info_copy(info, &parsed);
    return 1;
}
