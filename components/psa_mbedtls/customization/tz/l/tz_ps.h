
enum psa_storage_status_e{
	PS_STATUS_EMPTY = 0,
	PS_STATUS_INIT,
	PS_STATUS_ERR,
};

/* key storage */
#define KEY_STORAGE_NUM_MAX (16)
#define KEY_NAME_LENGTH_MAX (16)

/**
 * @brief key protect storage
 *
 * key protect storage
 *
 * @attention
 * - This API is used to find key id by name
 *
 * @param
 * -names[in]: the key name
 * -id[out]  : if the key exists, return the keyid
 * @return
 *  - 0: succeed
 *  - others: other errors.
 */
int ps_key_get_id_by_name(char *names, uint32_t *id);

/**
 * @brief key protect storage
 *
 * key protect storage
 *
 * @attention
 * - This API is used to generate a key by attribute, and save the key name and ID in the ps module.
 *
 * @param
 * -names[in]  : specify key name, must less than KEY_NAME_LENGTH_MAX
 * -key_id[in] : specify key id, 0 on failure, generate number less than KEY_STORAGE_NUM_MAX
 * -type[in]   : The key type to write
 * -bits[in]   : The key size in bits.  Keys of size 0 are not supported
 * -usage[in]  : The usage flags to write, see macro psa_key_usage_t
 * -alg[in]    : The permitted algorithm policy to write, see macro psa_algorithm_t
 * @return
 *  - 0: succeed
 *  - others: other errors.
 */
int ps_key_generate_by_attribute(char *names, uint32_t key_id, uint16_t type, uint16_t bits, uint32_t usage, uint32_t alg);
/**
 * @brief key protect storage
 *
 * key protect storage
 *
 * @attention
 * - This API is used to destroy the key by id.
 *
 * @param
 * -id[in]  : key id
 * @return
 *  - 0: succeed
 *  - others: other errors.
 */
int ps_key_destroy_by_id(uint32_t id);
/**
 * @brief key protect storage
 *
 * key protect storage
 *
 * @attention
 * - This API is used to destroy the key by name.
 *
 * @param
 * -names[in]: the key name
 * @return
 *  - 0: succeed
 *  - others: other errors.
 */
int ps_key_destroy_by_names(char *names);