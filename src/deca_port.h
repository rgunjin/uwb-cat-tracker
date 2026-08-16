#ifndef DECA_PORT_H
#define DECA_PORT_H

/*!
 * @brief Check that the SPI bus and reset GPIO drivers are ready.
 * @return 0 on success, -ENODEV if a driver has not initialised.
 */
int deca_port_init(void);

/*!
 * @brief Pulse the DW1000 reset line.
 *
 * RSTn is open-drain: it is pulled low and then released to
 * high-impedance. It must never be driven high.
 */
void reset_DW1000(void);

/*! @brief Switch the SPI clock to 2 MHz, required before PLL lock. */
void port_set_dw1000_slowrate(void);

/*! @brief Switch the SPI clock to 8 MHz. */
void port_set_dw1000_fastrate(void);

#endif /* DECA_PORT_H */
