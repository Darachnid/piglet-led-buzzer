#pragma once
#include <Arduino.h>
#include "OtaProto.h"

// JCMK_CHANNELS table layout in MeshNode.cpp / PigletNode (2.4 GHz = indices 0..13).
static const uint8_t PIGLET_JCMK_24G_LAST_IDX = 13;
static const uint8_t PIGLET_JCMK_NUM_CHANNELS = 40;

inline bool pigletBoardRf24Only(uint8_t boardType) {
  return boardType == PIGLET_BOARD_C6 || boardType == PIGLET_BOARD_S3_NODE;
}

// Highest JCMK_CHANNELS[] index this board type may be assigned. Unknown Piglet
// markers default to dual-band (legacy C5 headless / XIAO C5).
inline uint8_t pigletBoardMaxJcmkChIdx(uint8_t boardType, bool isPiglet) {
  if (!isPiglet) return PIGLET_JCMK_NUM_CHANNELS - 1;
  if (pigletBoardRf24Only(boardType)) return PIGLET_JCMK_24G_LAST_IDX;
  return PIGLET_JCMK_NUM_CHANNELS - 1;
}

inline uint16_t pigletBoardExpectedOtaChipId(uint8_t boardType) {
  switch (boardType) {
    case PIGLET_BOARD_C6:      return 13;  // ESP32-C6
    case PIGLET_BOARD_S3_NODE: return 9;   // ESP32-S3
    case PIGLET_BOARD_NODE:
    case PIGLET_BOARD_TDONGLE: return 23;  // ESP32-C5
    case PIGLET_BOARD_XIAO:    return 0;   // ambiguous silicon — reject OTA unless board-specific image
    default:                   return 0xFFFF;
  }
}
