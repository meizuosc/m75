#ifndef ES705_RWDB_H
#define ES705_RWDB_H

/* MQ100 states */
#define MQ100_STATE_RESET (0)
#define MQ100_STATE_NORMAL (1)

struct es705_rwdb_device_callbacks;

/**
 * Register rdb callbacks
 * @param  callbacks - structure containing
 * callbacks from es705 rdb/wdb driver
 * @return
 */
int es705_rdb_register(const struct es705_rwdb_device_callbacks *callbacks);

/**
 * Note: can we expect all of these
 * functions to be executed in process context?
 */
struct es705_rwdb_device_callbacks {
	/**
	 * cookie using for callbacks
	 */
	void *priv;

	/**
	 * Callback when firmware has been downloaded, device has been
	 * initialized and is ready for rdb/wdb
	 *
	 * @param  es705_priv - es705 private data. this cookie will be
	 * returned with all calls to es705_wdb
	 * @return on success, a pointer to the callee's private data,
	 *         this cookie must be returned with every other callback
	 *         on failure, return NULL
	 */
	void * (*probe)(void *es705_priv);

	/**
	 * This function is called when audience driver
	 * has detected the an interrupt from the device
	 * @param priv - cookie returned from probe()
	 */
	void (*intr)(void *priv);

	/**
	 * Callback whenever the device state changes.
	 * e.g. when firmware has been downloaded
	 * Use MQ100_STATE_XXX values for state param.
	 * @param priv - cookie returned from probe()
	 */
	void (*status)(void *priv, u8 state);
};

/*
 * Writes buf to es705 using wdb
 * this function will prepend 0x802F 0xffff
 * @param: buf - wdb data
 * @param: len - length
 * @return: no. of bytes written
 */
int es705_wdb(const void *buf, int len);

/*
 * Reads buf from es705 using rdb
 * @param:  buf - rdb data Max Size supported
 * - 2*PAGE_SIZE Ensure buffer allocated has enough space
 *			for rdb
 * @param:  id - type specifier
 * @return: no. of bytes read
 */
int es705_rdb(void *buf, int id);

int es705_rdb_register(const struct es705_rwdb_device_callbacks *callbacks);

#endif

