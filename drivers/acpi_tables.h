#ifndef EFESOS_ACPI_TABLES_H
#define EFESOS_ACPI_TABLES_H

#define ACPI_RSDP_V1_LENGTH 20U
#define ACPI_RSDP_V2_LENGTH 36U
#define ACPI_SDT_HEADER_LENGTH 36U
#define ACPI_HPET_TABLE_MIN_LENGTH 56U
#define ACPI_MADT_HEADER_LENGTH 44U
#define ACPI_MAX_RSDP_LENGTH 4096U
#define ACPI_MAX_SDT_LENGTH 65536U
#define ACPI_MAX_RSDT_ENTRIES 256U
#define ACPI_MADT_MAX_PROCESSORS 256U
#define ACPI_MADT_MAX_IO_APICS 8U
#define ACPI_MADT_ISA_IRQ_COUNT 16U

struct acpi_rsdp_info {
    unsigned int rsdt_address;
    unsigned int xsdt_address_low;
    unsigned int xsdt_address_high;
    unsigned int revision;
    unsigned int length;
};

struct acpi_hpet_table_info {
    unsigned int event_timer_block_id;
    unsigned int physical_address;
    unsigned int minimum_tick;
    unsigned char sequence_number;
    unsigned char page_protection;
};

struct acpi_madt_io_apic {
    unsigned int id;
    unsigned int physical_address;
    unsigned int global_interrupt_base;
};

struct acpi_madt_irq_override {
    unsigned int global_interrupt;
    unsigned int flags;
    unsigned char present;
};

struct acpi_madt_table_info {
    unsigned int local_apic_address;
    unsigned int flags;
    unsigned int enabled_local_apics;
    unsigned int enabled_x2apics;
    unsigned int io_apic_count;
    struct acpi_madt_io_apic io_apics[ACPI_MADT_MAX_IO_APICS];
    struct acpi_madt_irq_override
        isa_overrides[ACPI_MADT_ISA_IRQ_COUNT];
};

int acpi_checksum_valid(const unsigned char *bytes, unsigned int length);
int acpi_parse_rsdp(const unsigned char *bytes, unsigned int available,
    struct acpi_rsdp_info *info);
int acpi_sdt_peek_length(const unsigned char *bytes, unsigned int available,
    const char signature[4], unsigned int *length);
int acpi_parse_sdt(const unsigned char *bytes, unsigned int available,
    const char signature[4], unsigned int *length);
int acpi_rsdt_entry_at(const unsigned char *bytes, unsigned int available,
    unsigned int index, unsigned int *physical_address,
    unsigned int *entry_count);
int acpi_xsdt_entry_at(const unsigned char *bytes, unsigned int available,
    unsigned int index, unsigned int *physical_address_low,
    unsigned int *physical_address_high, unsigned int *entry_count);
int acpi_parse_hpet_table(const unsigned char *bytes, unsigned int available,
    struct acpi_hpet_table_info *info);
int acpi_parse_madt_table(const unsigned char *bytes, unsigned int available,
    struct acpi_madt_table_info *info);

#endif
