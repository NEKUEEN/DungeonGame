# Changes — Stats System Fix

## ไฟล์ที่แก้ไข: Player.hpp, Player.cpp, Game.hpp, Game.cpp

### สิ่งที่เปลี่ยน

**Player.hpp — Stats struct**
- `body` → ลบออก (ไม่ใช้เป็น field คงที่แล้ว)
- เพิ่ม `maxHp`, `baseAtk`, `baseDef`, `baseDodge` — base stats สำหรับคำนวณ body
- เพิ่ม `maxMentality` — ค่าสูงสุด แยกจาก `mentality` (current)
- เพิ่ม `hpDepleted` — flag สำหรับ trigger mentality drain
- เพิ่ม inline functions `computeBody()` และ `computeBattleIndex()`

**Body formula:**
  body = maxHp×0.4 + ATK×0.3 + DEF×0.2 + Dodge×0.1
  (ใช้ max stats ทั้งหมด — ไม่ลดตาม damage)

**Battle Index formula:**
  battleIndex = body + maxMentality + itemLevel × bonusPct
  (ไม่ขยับเลยถ้าไม่ได้ level up หรือเปลี่ยน equipment)

**Mentality drain:**
- HP ลดถึง 0 → hpDepleted = true → log แจ้ง
- ทุก turn ที่ผ่านไปขณะ hpDepleted → mentality -= 3
- mentality <= 0 → ตาย

**Game.cpp — helper methods**
- `getBody()` — คำนวณ body real-time
- `getBattleIndex()` — คำนวณ battle index real-time
- `getItemLevel()` — รวม item value จาก equipment
- Status panel แสดงค่าจาก compute functions ทั้งหมด

## ไฟล์ที่ไม่ได้แก้ (copy จาก project เดิม)
TileMap, Enemy, FogOfWar, Item, Inventory, Equipment, CoreSlots,
DropTable, MonsterDB, TextureManager
