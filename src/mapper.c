#include "mapper.h"
#include "alloc.h"
#include "cartridge.h"
#include "logging.h"
#include "system.h"

#include "mappers/000_nrom.h"
#include "mappers/001_mmc1.h"
#include "mappers/003_cnrom.h"
#include "mappers/004_mmc3.h"
#include "mappers/005_mmc5.h"
#include "mappers/007_axrom.h"
#include "mappers/009_mmc2.h"
#include "mappers/071_camerica.h"
#include "mappers/163_fc001.h"

static void generic_destroy(void* _map) { free(_map); }

// Polymorphic Mapper
Mapper* mapper_build(Cartridge* cart)
{
    Mapper* map = malloc_or_fail(sizeof(Mapper));
    switch (cart->mapper) {
        case 0:
        case 2: {
            NROM* nrom       = NROM_build(cart);
            map->obj         = nrom;
            map->name        = "NROM";
            map->step        = NULL;
            map->read        = &NROM_read;
            map->write       = &NROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &NROM_serialize;
            map->deserialize = &NROM_deserialize;
            break;
        }
        case 1: {
            MMC1* mmc1       = MMC1_build(cart);
            map->obj         = mmc1;
            map->name        = "MMC1";
            map->step        = NULL;
            map->read        = &MMC1_read;
            map->write       = &MMC1_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC1_serialize;
            map->deserialize = &MMC1_deserialize;
            break;
        }
        case 3: {
            CNROM* cnrom     = CNROM_build(cart);
            map->obj         = cnrom;
            map->name        = "CNROM";
            map->step        = NULL;
            map->read        = &CNROM_read;
            map->write       = &CNROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &CNROM_serialize;
            map->deserialize = &CNROM_deserialize;
            break;
        }
        case 4: {
            MMC3* mmc3       = MMC3_build(cart);
            map->obj         = mmc3;
            map->name        = "MMC3";
            map->step        = &MMC3_step;
            map->read        = &MMC3_read;
            map->write       = &MMC3_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC3_serialize;
            map->deserialize = &MMC3_deserialize;
            break;
        }
        case 5: {
            MMC5* mmc5       = MMC5_build(cart);
            map->obj         = mmc5;
            map->name        = "MMC5";
            map->step        = &MMC5_step;
            map->read        = &MMC5_read;
            map->write       = &MMC5_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC5_serialize;
            map->deserialize = &MMC5_deserialize;
            break;
        }
        case 7: {
            AxROM* axrom     = AxROM_build(cart);
            map->obj         = axrom;
            map->name        = "AxROM";
            map->step        = NULL;
            map->read        = &AxROM_read;
            map->write       = &AxROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &AxROM_serialize;
            map->deserialize = &AxROM_deserialize;
            break;
        }
        case 9: {
            MMC2* mmc2       = MMC2_build(cart);
            map->obj         = mmc2;
            map->name        = "MMC2";
            map->step        = NULL;
            map->read        = &MMC2_read;
            map->write       = &MMC2_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC2_serialize;
            map->deserialize = &MMC2_deserialize;
            break;
        }
        case 71: {
            Map071* map071   = Map071_build(cart);
            map->obj         = map071;
            map->name        = "Map071";
            map->step        = NULL;
            map->read        = &Map071_read;
            map->write       = &Map071_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &Map071_serialize;
            map->deserialize = &Map071_deserialize;
            break;
        }
        case 163: {
            FC_001* fc_001   = FC_001_build(cart);
            map->obj         = fc_001;
            map->name        = "FC_001";
            map->step        = &FC_001_step;
            map->read        = &FC_001_read;
            map->write       = &FC_001_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &FC_001_serialize;
            map->deserialize = &FC_001_deserialize;
            break;
        }
        default:
            panic("unsupported mapper %u", cart->mapper);
    }
    return map;
}

void mapper_destroy(Mapper* map)
{
    map->destroy(map->obj);
    free(map);
}

uint8_t mapper_read(Mapper* map, uint16_t addr)
{
    return map->read(map->obj, addr);
}

void mapper_write(Mapper* map, uint16_t addr, uint8_t value)
{
    return map->write(map->obj, addr, value);
}

void mapper_step(Mapper* map, System* sys)
{
    if (map->step)
        map->step(map->obj, sys);
}

void mapper_serialize(Mapper* map, FILE* fout)
{
    return map->serialize(map->obj, fout);
}

void mapper_deserialize(Mapper* map, FILE* fin)
{
    map->deserialize(map->obj, fin);
}
