/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board.h"
#include "pico/bootrom.h"
#include "tusb.h"
#include "profile.h"
#include "nxjson.h"

// #define DEBUG_MSC = 1

extern void set_profile(uint32_t selected_profile_number);

extern volatile int16_t angle;
extern profile_t profiles[9];
extern uint32_t selected_profile;

static bool ejected = false;
static int received_profile_number = -1;


#define README_CONTENTS \
"Editing Wheel\n\n\
See more at https://github.com/Abstract-Horizon/editing-wheel\n\n\
Files: \n\
- README.TXT   - this file \n\
- PROFILE.TXT  - selected profile - only one char (1-9).\n\
    Write to it to select profile.\n\
    Read to see selected profile\n\
- PROFILEx.TXT - JSON for profile 'x' (x in 1-9)\n\
    Write to it to override existing values.\n\
- ANGLE.TXT    - Current position of the wheel\n\
    Note: filesystem is cached so it doesn't\n\
    really represent current value\n\
"
#define README_CONTENTS_SIZE_L (sizeof(README_CONTENTS) - 1) & 0xFF
#define README_CONTENTS_SIZE_H ((sizeof(README_CONTENTS) - 1) & 0xFF00) >> 8


enum
{
  DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};


uint8_t local_buffer[DISK_BLOCK_SIZE] = {};

//------------- Block0: Boot Sector -------------//
// byte_per_sector    = DISK_BLOCK_SIZE; fat12_sector_num_16  = DISK_BLOCK_NUM;
// sector_per_cluster = 1; reserved_sectors = 1;
// fat_num            = 1; fat12_root_entry_num = 16;
// sector_per_fat     = 1; sector_per_track = 1; head_num = 1; hidden_sectors = 0;
// drive_number       = 0x80; media_type = 0xf8; extended_boot_signature = 0x29;
// filesystem_type    = "FAT12   "; volume_serial_number = 0x1234; volume_label = "TinyUSB MSC";
// FAT magic code at offset 510-511
const uint8_t boot_sector_block_data[] = {
    0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00,
    0x01, 0x10, 0x00, 0x10, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 'E' , 'W' , 'h' , 'e' , 'e' ,
    'l' , ' ' , ' ' , ' ' , ' ' , ' ' , 0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,
};

const uint8_t boot_second_footer_data[] = { 0x55, 0xAA };

const uint8_t fat_block_data[] = {
    0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

    // 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // // first 2 entries must be F8FF, third entry is cluster end of readme file
};

const uint8_t root_dir_volume_name_data[] = {
    'E' , 'W' , 'h' , 'e' , 'e' , 'l' , ' ' , ' ' , ' ' , ' ' , ' ' , 0x08, 0x00, 0x00, 0x00, 0x00,
    0x41, 0x55, 0x41, 0x55, 0x00, 0x00, 0x75, 0x79, 0x41, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t root_dir_readme_file_data[] = {
    'R' , 'E' , 'A' , 'D' , 'M' , 'E' , ' ' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0xC6, 0x52, 0x6D,
    0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00, README_CONTENTS_SIZE_L, README_CONTENTS_SIZE_H, 0x00, 0x00, // readme's files size (4 Bytes)
};

const uint8_t root_dir_entry_template[] = {
    'A' , 'N' , 'G' , 'L' , 'E' , ' ' , ' ' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0x1A, 0x27, 0x6E,
    0x41, 0x55, 0x41, 0x55, 0x00, 0x00, 0x27, 0x6E, 0x41, 0x55, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00,
};

const uint8_t root_dir_profile_file_data[] = {
    'P' , 'R' , 'O' , 'F' , 'I' , 'L' , 'E' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0xC6, 0x52, 0x6D,
    0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x0D, 0x00, 1, 0x00, 0x00, 0x00, // readme's files size (4 Bytes)
};



const uint8_t readme_block_data[] = README_CONTENTS;

#define PROFILE_JSON_TEMPLATE "{\n\
  \"direction\": % 1i,\n\
  \"zero\": %03i,\n\
  \"dividers\": %02i,\n\
  \"axis\": %02i,\n\
  \"expo\": % 02.3f,\n\
  \"gain\": %02.3f,\n\
  \"dead_band\": %02.3f,\n\
}\n"


ssize_t output_profile(uint8_t* buffer, int profile_no) {
    return sprintf(
        buffer, PROFILE_JSON_TEMPLATE,
        profiles[profile_no].direction,
        profiles[profile_no].zero,
        profiles[profile_no].dividers,
        profiles[profile_no].axis,
        profiles[profile_no].expo,
        profiles[profile_no].gain_factor,
        profiles[profile_no].dead_band
    );
}


// --------------------------------------------------------------------

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;

    const char vid[] = "EWheel";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id  , vid, strlen(vid));
    memcpy(product_id , pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;

    // RAM disk is ready until ejected
    if (ejected) {
        // Additional Sense 3A-00 is NOT_FOUND
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {

    (void) lun;

    *block_count = DISK_BLOCK_NUM;
    *block_size  = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun;
  (void) power_condition;

  if (load_eject) {
      if (start) {
          // load disk storage
      } else {
          // unload disk storage
          ejected = true;
      }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;

    // out of ramdisk
    if (lba >= DISK_BLOCK_NUM) { return -1; }

    switch (lba) {
      case (0): {
          memcpy(buffer, boot_sector_block_data, sizeof(boot_sector_block_data));
          memcpy(buffer + 510, boot_second_footer_data, 2);
      }
      break;
      case (1): {
          memcpy(buffer, fat_block_data, sizeof(fat_block_data));
      }
      break;
      case (2): {
          // ensure all entries are 'free' before copying static data over it
          for (int i = 0; i < 16; i++) {
              local_buffer[i * 32] = 0;
          }
          memcpy(local_buffer, root_dir_volume_name_data, sizeof(root_dir_volume_name_data));
          memcpy(local_buffer + 32, root_dir_readme_file_data, sizeof(root_dir_readme_file_data));
          memcpy(local_buffer + 64, root_dir_entry_template, sizeof(root_dir_entry_template));
          for (int i = 1; i < 10; i++) {
              memcpy(local_buffer + 64 + 32 * i, root_dir_entry_template, sizeof(root_dir_entry_template));
              sprintf(local_buffer + 64 + 32 * i, "PROFILE%iTXT", i);
              ssize_t len = output_profile(NULL, i - 1);

              local_buffer[64 + 32 * i + 26] = i + 3;
              local_buffer[64 + 32 * i + 27] = 0;
              local_buffer[64 + 32 * i + 28] = (uint8_t)(len & 0xff);
              local_buffer[64 + 32 * i + 29] = (uint8_t)((len & 0xff00) >> 8);
              local_buffer[64 + 32 * i + 30] = 0;
              local_buffer[64 + 32 * i + 31] = 0;
          }
          memcpy(local_buffer + 32 * 11, root_dir_profile_file_data, sizeof(root_dir_profile_file_data));
          memcpy(buffer, local_buffer, bufsize);
      }
      break;
      case (3): {
          memcpy(buffer, readme_block_data, sizeof(readme_block_data));
      }
      break;
      case (4): {
          sprintf(local_buffer, "%04d", angle);
          memcpy(buffer, local_buffer, bufsize);
      }
      break;
      case 5 ... 13: {
          int pi = lba - 5;
          output_profile(local_buffer, pi);
          memcpy(buffer, local_buffer, bufsize);
      }
      break;
      case (14): {
          sprintf(local_buffer, "%1d", selected_profile);
          memcpy(buffer, local_buffer, bufsize);
      }
      break;
      case (15): {

      }
      break;
      default: break;
    }
#ifdef DEBUG_MSC
    printf("Received read lba=%i, offset=%i\n", lba, offset);
#endif
    return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun) {
    (void) lun;

    // READONLY
    // return false;
    return true;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun;

    // out of ramdisk
    if (lba >= DISK_BLOCK_NUM) { return -1; }

    switch (lba) {
      break;
      case (2): {
        if (received_profile_number >= 0 && received_profile_number <= 8) {
          uint8_t ptr = 64 + 32 + received_profile_number * 32 + 28;

          ssize_t profile_json_size = buffer[ptr] + 256 * buffer[ptr + 1];
          printf("Received profile %i with size %i\n", received_profile_number + 1, profile_json_size);

          if (profile_json_size > 511) { profile_json_size = 511; }
          local_buffer[profile_json_size] = 0x0; // Make it null terminated


#ifdef DEBUG_MSC
          printf("Got: \"%.*s\"\n", profile_json_size, local_buffer);
#endif
          const nx_json* json = nx_json_parse(local_buffer, 0);
          if (json) {
              profiles[received_profile_number].direction = nx_json_get(json, "direction")->num.s_value;
              profiles[received_profile_number].zero = nx_json_get(json, "zero")->num.s_value;
              profiles[received_profile_number].dividers = nx_json_get(json, "dividers")->num.s_value;
              profiles[received_profile_number].axis = nx_json_get(json, "axis")->num.s_value;
              profiles[received_profile_number].expo = nx_json_get(json, "expo")->num.dbl_value;
              profiles[received_profile_number].gain_factor = nx_json_get(json, "gain")->num.dbl_value;
              profiles[received_profile_number].dead_band = nx_json_get(json, "dead_band")->num.dbl_value;

              printf("direction=%d\n", profiles[received_profile_number].direction);
              printf("zero=%d\n",      profiles[received_profile_number].zero);
              printf("dividers=%d\n",  profiles[received_profile_number].dividers);
              printf("axis=%d\n",      profiles[received_profile_number].axis);
              printf("expo=%f\n",      profiles[received_profile_number].expo);
              printf("gain=%f\n",      profiles[received_profile_number].gain_factor);
              printf("dead_band=%f\n", profiles[received_profile_number].dead_band);
              nx_json_free(json);

            //   set_profile(received_profile_number);
          }

          received_profile_number = -1;
        }
      }
      break;
      case 5 ... 13: {
          received_profile_number = lba - 5;
          memcpy(local_buffer, buffer, bufsize);
      }
      break;
      case 14: {
          if (buffer[0] == 'R') {
              reset_usb_boot(0, 0);
          } else {
                uint32_t selection = buffer[0] - '1';
                if (selection >= 0 && selection < 9) {
                    set_profile(selection);
                    printf("Selected profile %d", selection);

                }
          }
      }
      break;
      default: break;
    }


    // TODO not readonly and not writable
    // uint8_t* addr = msc_disk[lba] + offset;
    // memcpy(addr, buffer, bufsize);

    (void) lba; (void) offset; (void) buffer;
#ifdef DEBUG_MSC
    printf("Received write lba=%i, offset=%i\n", lba, offset);
#endif
    return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    // read10 & write10 has their own callback and MUST not be handled here

    void const* response = NULL;
    int32_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
      default:
        // Set Sense = Invalid Command Operation
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

        // negative means error -> tinyusb could stall and/or response with failed status
        resplen = -1;
      break;
    }

    // return resplen must not larger than bufsize
    if (resplen > bufsize) {
        resplen = bufsize;
    }

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, (size_t) resplen);
        } else {
            // SCSI output
        }
    }

    return (int32_t) resplen;
}
