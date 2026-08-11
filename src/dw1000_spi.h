#ifndef DW1000_SPI_H
#define DW1000_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int dw1000_spi_init(void);
int dw1000_read(uint8_t reg, uint16_t off, uint8_t *data, size_t len);
int dw1000_write(uint8_t reg, uint16_t off, const uint8_t *data, size_t len);

#endif /* DW1000_SPI_H */
