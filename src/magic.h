#ifndef MAGIC_H
#define MAGIC_H

#include <cstdint>

namespace Voraxis {
namespace Magic {

void initMagicTables();

uint64_t getBishopAttacks(int sq, uint64_t occ);
uint64_t getRookAttacks(int sq, uint64_t occ);
uint64_t getQueenAttacks(int sq, uint64_t occ);

} // namespace Magic
} // namespace Voraxis

#endif // MAGIC_H
