/*
 * Microsemi Switchtec PCIe Driver
 * Copyright (c) 2016, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef SWITCHTEC_H
#define SWITCHTEC_H

#include <linux/pci.h>
#include <linux/cdev.h>

#define MICROSEMI_VENDOR_ID         0x11f8
#define MICROSEMI_NTB_CLASSCODE     0x068000
#define MICROSEMI_MGMT_CLASSCODE    0x058000

#define SWITCHTEC_MRPC_PAYLOAD_SIZE 1024

enum {
	SWITCHTEC_GAS_MRPC_OFFSET       = 0x0000,
	SWITCHTEC_GAS_TOP_CFG_OFFSET    = 0x1000,
	SWITCHTEC_GAS_SW_EVENT_OFFSET   = 0x1800,
	SWITCHTEC_GAS_SYS_INFO_OFFSET   = 0x2000,
	SWITCHTEC_GAS_FLASH_INFO_OFFSET = 0x2200,
	SWITCHTEC_GAS_PART_CFG_OFFSET   = 0x4000,
	SWITCHTEC_GAS_NTB_OFFSET        = 0x10000,
	SWITCHTEC_GAS_PFF_CSR_OFFSET    = 0x134000,
};

struct mrpc_regs {
	u8 input_data[SWITCHTEC_MRPC_PAYLOAD_SIZE];
	u8 output_data[SWITCHTEC_MRPC_PAYLOAD_SIZE];
	u32 cmd;
	u32 status;
	u32 ret_value;
} __packed;

enum mrpc_status {
	SWITCHTEC_MRPC_STATUS_INPROGRESS = 1,
	SWITCHTEC_MRPC_STATUS_DONE = 2,
	SWITCHTEC_MRPC_STATUS_ERROR = 0xFF,
	SWITCHTEC_MRPC_STATUS_INTERRUPTED = 0x100,
};

struct sys_info_regs {
	uint32_t device_id;
	uint32_t device_version;
	uint32_t firmware_version;
	uint32_t reserved1;
	uint32_t vendor_table_revision;
	uint32_t table_format_version;
	uint32_t partition_id;
	uint32_t cfg_file_fmt_version;
	uint32_t reserved2[58];
	char     vendor_id[8];
	char     product_id[16];
	char     product_revision[4];
	char     component_vendor[8];
	uint16_t component_id;
	uint8_t  component_revision;
};

struct flash_info_regs {
	uint32_t flash_part_map_upd_idx;

	struct partition_info {
		uint32_t address;
		uint32_t build_version;
		uint32_t build_string;
	} active_main_fw;

	struct partition_info active_cfg;
	struct partition_info inactive_main_fw;
	struct partition_info inactive_cfg;
};

struct ntb_info_regs {
	u8  partition_count;
	u8  partition_id;
	u16 reserved1;
	u64 ep_map;
	u16 requester_id;
} __packed;

struct part_cfg_regs {
	u32 status;
	u32 state;
	u32 port_cnt;
	u32 usp_port_mode;
	u32 usp_pff_inst_id;
	u32 vep_pff_inst_id;
	u32 dsp_inst_id[47];
	u32 reserved1[11];
	u16 vep_vector_number;
	u16 usp_vector_number;
	u32 port_event_bitmap;
	u32 reserved2[3];
	u32 part_event_summary;
	u32 reserved3[3];
	u32 part_reset_event_hdr;
	u8  part_reset_event_data[20];
	u32 mrpc_completion_hdr;
	u8  mrpc_completion_data[20];
	u32 mrpc_completion_async_hdr;
	u8  mrpc_completion_async_data[20];
	u32 dynamic_part_binding_evt_hdr;
	u8 dynamic_part_binding_evt_data[20];
	u32 reserved4[159];
} __packed;

enum {
	SWITCHTEC_PART_CFG_EVENT_MRPC_CMP = 2,
	SWITCHTEC_PART_CFG_EVENT_MRPC_ASYNC_CMP = 4,
};

struct switchtec_dev {
	struct pci_dev *pdev;
	struct msix_entry msix[4];
	struct device dev;
	struct cdev cdev;
	unsigned int event_irq;

	void __iomem *mmio;
	struct mrpc_regs __iomem *mmio_mrpc;
	struct sys_info_regs __iomem *mmio_sys_info;
	struct flash_info_regs __iomem *mmio_flash_info;
	struct ntb_info_regs __iomem *mmio_ntb;
	struct part_cfg_regs __iomem *mmio_part_cfg;

	/*
	 * The mrpc mutex must be held when accessing the other
	 * mrpc fields
	 */
	struct mutex mrpc_mutex;
	struct list_head mrpc_queue;
	int mrpc_busy;
	struct work_struct mrpc_work;
	struct delayed_work mrpc_timeout;
};

#endif
