/* -----------------------------------------------------------------------------
 * Copyright (C) 2008 Robert Ernst <robert.ernst@linux-solutions.at>
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See COPYING for GPL licensing information.
 */



#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "mini_snmpd.h"



/* -----------------------------------------------------------------------------
 * Module variables
 *
 * To extend the MIB, add the definition of the SNMP table here. Note that the
 * variables use OIDs that have two subids more, which both are specified in the
 * mib_build_entry() and mib_build_entries() function calls. For example, the
 * system table uses the OID .1.3.6.1.2.1.1, the first system table variable,
 * system.sysDescr.0 (using OID .1.3.6.1.2.1.1.1.0) is appended to the MIB using
 * the function call mib_build_entry(&m_system_oid, 1, 0, ...).
 *
 * The first parameter is the array containing the list of subids (up to 14 here),
 * the next is the number of subids. The last parameter is the length that this
 * OID will need encoded in SNMP packets (including the BER type and length fields).
 */



static const oid_t m_sdk_oid            = { { 1, 3, 6, 1, 4, 1, 126,3         }, 8, 10 };



#ifdef __DEMO__
static const oid_t m_demo_oid		= { { 1, 3, 6, 1, 4, 1, 125		}, 7, 10 };
#endif

static const int m_load_avg_times[3] = { 1, 5, 15 };



/* -----------------------------------------------------------------------------
 * Helper functions for encoding values
 */

static int encode_snmp_element_integer(value_t *value, int integer_value)
{
	unsigned char *buffer;
	int length;

	buffer = value->data.buffer;
	if (integer_value < -8388608 || integer_value > 8388607) {
		length = 4;
	} else if (integer_value < -32768 || integer_value > 32767) {
		length = 3;
	} else if (integer_value < -128 || integer_value > 127) {
		length = 2;
	} else {
		length = 1;
	}
	*buffer++ = BER_TYPE_INTEGER;
	*buffer++ = length;
	while (length--) {
		*buffer++ = ((unsigned int)integer_value >> (8 * length)) & 0xFF;
	}
	value->data.encoded_length = buffer - value->data.buffer;
	return 0;
}

static int encode_snmp_element_string(value_t *value, const char *string_value)
{
	unsigned char *buffer;
	int length;

	buffer = value->data.buffer;
	length = strlen(string_value);
	*buffer++ = BER_TYPE_OCTET_STRING;
	if (length > 65535) {
		lprintf(LOG_ERR, "could not encode '%s': string overflow\n", string_value);
		return -1;
	} else if (length > 255) {
		*buffer++ = 0x82;
		*buffer++ = (length >> 8) & 0xFF;
		*buffer++ = length & 0xFF;
	} else if (length > 127) {
		*buffer++ = 0x81;
		*buffer++ = length & 0xFF;
	} else {
		*buffer++ = length & 0x7F;
	}
	while (*string_value) {
		*buffer++ = (unsigned char)(*string_value++);
	}
	value->data.encoded_length = buffer - value->data.buffer;
	return 0;
}

static int encode_snmp_element_oid(value_t *value, const oid_t *oid_value)
{
	unsigned char *buffer;
	int length;
	int i;

	if (oid_value == NULL) {
		return -1;
	}
	buffer = value->data.buffer;
	length = 1;
	for (i = 2; i < oid_value->subid_list_length; i++) {
		if (oid_value->subid_list[i] >= (1 << 28)) {
			length += 5;
		} else if (oid_value->subid_list[i] >= (1 << 21)) {
			length += 4;
		} else if (oid_value->subid_list[i] >= (1 << 14)) {
			length += 3;
		} else if (oid_value->subid_list[i] >= (1 << 7)) {
			length += 2;
		} else {
			length += 1;
		}
	}
	*buffer++ = BER_TYPE_OID;
	if (length > 0xFFFF) {
		lprintf(LOG_ERR, "could not encode '%s': oid overflow\n", oid_ntoa(oid_value));
		return -1;
	} else if (length > 0xFF) {
		*buffer++ = 0x82;
		*buffer++ = (length >> 8) & 0xFF;
		*buffer++ = length & 0xFF;
	} else if (length > 0x7F) {
		*buffer++ = 0x81;
		*buffer++ = length & 0xFF;
	} else {
		*buffer++ = length & 0x7F;
	}
	*buffer++ = oid_value->subid_list[0] * 40 + oid_value->subid_list[1];
	for (i = 2; i < oid_value->subid_list_length; i++) {
		if (oid_value->subid_list[i] >= (1 << 28)) {
			length = 5;
		} else if (oid_value->subid_list[i] >= (1 << 21)) {
			length = 4;
		} else if (oid_value->subid_list[i] >= (1 << 14)) {
			length = 3;
		} else if (oid_value->subid_list[i] >= (1 << 7)) {
			length = 2;
		} else {
			length = 1;
		}
		while (length--) {
			if (length) {
				*buffer++ = ((oid_value->subid_list[i] >> (7 * length)) & 0x7F) | 0x80;
			} else {
				*buffer++ = (oid_value->subid_list[i] >> (7 * length)) & 0x7F;
			}
		}
	}
	value->data.encoded_length = buffer - value->data.buffer;
	return 0;
}

static int encode_snmp_element_unsigned(value_t *value, int type, unsigned int ticks_value)
{
	unsigned char *buffer;
	int length;

	buffer = value->data.buffer;
	if (ticks_value & 0xFF000000) {
		length = 4;
	} else if (ticks_value & 0x00FF0000) {
		length = 3;
	} else if (ticks_value & 0x0000FF00) {
		length = 2;
	} else {
		length = 1;
	}
	*buffer++ = type;
	*buffer++ = length;
	while (length--) {
		*buffer++ = (ticks_value >> (8 * length)) & 0xFF;
	}
	value->data.encoded_length = buffer - value->data.buffer;
	return 0;
}



/* -----------------------------------------------------------------------------
 * Helper functions for the MIB
 */

static int mib_build_entry(const oid_t *prefix, int column, int row, int type,
	const void *default_value)
{
	value_t *value;
	int length;
	int i;

	/* Create a new entry in the MIB table */
	if (g_mib_length < MAX_NR_VALUES) {
		value = &g_mib[g_mib_length++];
	} else {
		lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': table overflow\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}

	/* Create the OID from the prefix, the column and the row */
	memcpy(&value->oid, prefix, sizeof (value->oid));
	if (value->oid.subid_list_length < MAX_NR_SUBIDS) {
		value->oid.subid_list[value->oid.subid_list_length++] = column;
	} else {
		lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': oid overflow\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}
	if (value->oid.subid_list_length < MAX_NR_SUBIDS) {
		value->oid.subid_list[value->oid.subid_list_length++] = row;
	} else {
		lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': oid overflow\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}

	/* Calculate the encoded length of the created OID (note: first the length
	 * of the subid list, then the length of the length/type header!)
	 */
	length = 1;
	for (i = 2; i < value->oid.subid_list_length; i++) {
		if (value->oid.subid_list[i] >= (1 << 28)) {
			length += 5;
		} else if (value->oid.subid_list[i] >= (1 << 21)) {
			length += 4;
		} else if (value->oid.subid_list[i] >= (1 << 14)) {
			length += 3;
		} else if (value->oid.subid_list[i] >= (1 << 7)) {
			length += 2;
		} else {
			length += 1;
		}
	}
	if (length > 0xFFFF) {
		lprintf(LOG_ERR, "could not encode '%s': oid overflow\n", oid_ntoa(&value->oid));
		return -1;
	} else if (length > 0xFF) {
		length += 4;
	} else if (length > 0x7F) {
		length += 3;
	} else {
		length += 2;
	}
	value->oid.encoded_length = length;

	/* Paranoia check against invalid default parameter (null pointer) */
	switch (type) {
		case BER_TYPE_OCTET_STRING:
		case BER_TYPE_OID:
			if (default_value == NULL) {
				lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': invalid default value\n",
					oid_ntoa(prefix), column, row);
				return -1;
			}
			break;
		default:
			break;
	}

	/* Create a data buffer for the value depending on the type:
	 *
	 * - strings and oids are assumed to be static or have the maximum allowed length
	 * - integers are assumed to be dynamic and don't have more than 32 bits
	 */
	switch (type) {
		case BER_TYPE_INTEGER:
			value->data.max_length = sizeof (int) + 2;
			value->data.encoded_length = 0;
			value->data.buffer = malloc(value->data.max_length);
			memset(value->data.buffer, 0, value->data.max_length);
			if (encode_snmp_element_integer(value, (int)default_value) == -1) {
				return -1;
			}
			break;
		case BER_TYPE_OCTET_STRING:
			value->data.max_length = strlen((const char *)default_value) + 4;
			value->data.encoded_length = 0;
			value->data.buffer = malloc(value->data.max_length);
			memset(value->data.buffer, 0, value->data.max_length);
			if (encode_snmp_element_string(value, (const char *)default_value) == -1) {
				return -1;
			}
			break;
		case BER_TYPE_OID:
			value->data.max_length = MAX_NR_SUBIDS * 5 + 4;
			value->data.encoded_length = 0;
			value->data.buffer = malloc(value->data.max_length);
			memset(value->data.buffer, 0, value->data.max_length);
			if (encode_snmp_element_oid(value, oid_aton((const char *)default_value)) == -1) {
				lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': invalid oid '%s'\n",
					oid_ntoa(prefix), column, row, (char *)default_value);
				return -1;
			}
			break;
		case BER_TYPE_COUNTER:
		case BER_TYPE_GAUGE:
		case BER_TYPE_TIME_TICKS:
			value->data.max_length = sizeof (unsigned int) + 2;
			value->data.encoded_length = 0;
			value->data.buffer = malloc(value->data.max_length);
			memset(value->data.buffer, 0, value->data.max_length);
			if (encode_snmp_element_unsigned(value, type, (unsigned int)default_value) == -1) {
				return -1;
			}
			break;
		default:
			lprintf(LOG_ERR, "could not create MIB entry '%s.%d.%d': unsupported type %d\n",
				oid_ntoa(prefix), column, row, type);
			return -1;
	}

	return 0;
}


static int mib_update_entry(const oid_t *prefix, int column, int row,
	int *pos, int type, const void *new_value)
{
	oid_t oid;

	/* Create the OID from the prefix, the column and the row */
	memcpy(&oid, prefix, sizeof (oid));
	if (oid.subid_list_length < MAX_NR_SUBIDS) {
		oid.subid_list[oid.subid_list_length++] = column;
	} else {
		lprintf(LOG_ERR, "could not update MIB entry '%s.%d.%d': oid overflow\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}
	if (oid.subid_list_length < MAX_NR_SUBIDS) {
		oid.subid_list[oid.subid_list_length++] = row;
	} else {
		lprintf(LOG_ERR, "could not update MIB entry '%s.%d.%d': oid overflow\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}

	/* Search the the MIB for the given OID beginning at the given position */
	while (*pos < g_mib_length) {
		if (g_mib[*pos].oid.subid_list_length == oid.subid_list_length
			&& !memcmp(g_mib[*pos].oid.subid_list, oid.subid_list,
				oid.subid_list_length * sizeof (oid.subid_list[0]))) {
			break;
		}
		*pos = *pos + 1;
	}
	if (*pos >= g_mib_length) {
		lprintf(LOG_ERR, "could not update MIB entry '%s.%d.%d': oid not found\n",
			oid_ntoa(prefix), column, row);
		return -1;
	}

	/* Paranoia check against invalid value parameter (null pointer) */
	switch (type) {
		case BER_TYPE_OCTET_STRING:
		case BER_TYPE_OID:
			if (new_value == NULL) {
				lprintf(LOG_ERR, "could not update MIB entry '%s.%d.%d': invalid default value\n",
					oid_ntoa(prefix), column, row);
				return -1;
			}
			break;
		default:
			break;
	}

	/* Update the data buffer for the value depending on the type. Note that we
	 * assume the buffer was allocated to hold the maximum possible value when
	 * the MIB was built!
	 */
	switch (type) {
		case BER_TYPE_INTEGER:
			if (encode_snmp_element_integer(&g_mib[*pos], (int)new_value) == -1) {
				return -1;
			}
			break;
		case BER_TYPE_OCTET_STRING:
			if (encode_snmp_element_string(&g_mib[*pos], (const char *)new_value) == -1) {
				return -1;
			}
			break;
		case BER_TYPE_OID:
			if (encode_snmp_element_oid(&g_mib[*pos], oid_aton((const char *)new_value)) == -1) {
				return -1;
			}
			break;
		case BER_TYPE_COUNTER:
		case BER_TYPE_GAUGE:
		case BER_TYPE_TIME_TICKS:
			if (encode_snmp_element_unsigned(&g_mib[*pos], type, (unsigned int)new_value) == -1) {
				return -1;
			}
			break;
		default:
			lprintf(LOG_ERR, "could not update MIB entry '%s.%d.%d': unsupported type %d\n",
				oid_ntoa(prefix), column, row, type);
			return -1;
	}

	return 0;
}



/* -----------------------------------------------------------------------------
 * Interface functions
 *
 * To extend the MIB, add the relevant mib_build_entry() calls (to add one MIB
 * variable) or mib_build_entries() calls (to add a column of a MIB table) in
 * the mib_build() function. Note that building the MIB must be done strictly in
 * ascending OID order or the SNMP getnext/getbulk functions will not work as
 * expected!
 *
 * To extend the MIB, add the relevant mib_update_entry() calls (to update one
 * MIB variable or one cell in a MIB table) in the mib_update() function. Note
 * that the MIB variables must be added in the correct order (i.e. ascending).
 * How to get the value for that variable is up to you, but bear in mind that
 * the mib_update() function is called between receiving the request from the
 * client and sending back the response; thus you should avoid time-consuming
 * actions!
 *
 * The variable types supported up to now are OCTET_STRING, INTEGER (32 bit
 * signed), COUNTER (32 bit unsigned), TIME_TICKS (32 bit unsigned, in 1/10s)
 * and OID.
 *
 * Note that the maximum number of MIB variables is restricted by the length of
 * the MIB array, (see mini_snmpd.h for the value of MAX_NR_VALUES).
 */

int mib_build(void)
{

	

	
#ifdef __DEMO__
	if (mib_build_entry(&m_demo_oid, 1, 0, BER_TYPE_INTEGER, (const void *)0) == -1	
		|| mib_build_entry(&m_demo_oid, 2, 0, BER_TYPE_INTEGER, (const void *)0) == -1) {
		return -1;
	}
#endif




	if (mib_build_entry(&m_sdk_oid, 1, 0, BER_TYPE_INTEGER, (const void *)0) == -1	
            || mib_build_entry(&m_sdk_oid, 1, 1, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 2, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 3, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 4, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 5, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 6, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 7, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 8, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 9, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 10, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 11, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 12, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 13, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 14, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 15, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 16, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 17, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 18, BER_TYPE_INTEGER, (const void *)0) == -1
            || mib_build_entry(&m_sdk_oid, 1, 19, BER_TYPE_INTEGER, (const void *)0) == -1 
	    || mib_build_entry(&m_sdk_oid, 2, 0, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 3, 0, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 4, 0, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 5, 0, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 6, 0, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 6, 1, BER_TYPE_INTEGER, (const void *)0) == -1
	    || mib_build_entry(&m_sdk_oid, 6, 2, BER_TYPE_INTEGER, (const void *)0) == -1 
	    || mib_build_entry(&m_sdk_oid, 6, 3, BER_TYPE_INTEGER, (const void *)0) == -1) {
		return -1;
	}





	return 0;
}

int mib_update(int full)
{
	union {
		

		sdkinfo_t sdkinfo;


#ifdef __DEMO__
		demoinfo_t demoinfo;
#endif
	} u;

	int pos;


	/* Begin searching at the first MIB entry */
	pos = 0;

	
#ifdef __DEMO__
	if (full) {
		get_demoinfo(&u.demoinfo);
		if (mib_update_entry(&m_demo_oid, 1, 0, &pos, BER_TYPE_INTEGER, (const void *)u.demoinfo.random_value_1) == -1
			|| mib_update_entry(&m_demo_oid, 2, 0, &pos, BER_TYPE_INTEGER, (const void *)u.demoinfo.random_value_2) == -1) {
			return -1;
		}
	}
#endif


	if (full) {
		get_sdkinfo(&u.sdkinfo);
		if (       mib_update_entry(&m_sdk_oid, 1, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_1) == -1
			|| mib_update_entry(&m_sdk_oid, 1, 1, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_2) == -1
			|| mib_update_entry(&m_sdk_oid, 1, 2, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_3) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 3, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_4) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 4, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_5) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 5, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_6) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 6, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_7) == -1   
                        || mib_update_entry(&m_sdk_oid, 1, 7, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_8) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 8, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_9) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 9, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_10) == -1   
                        || mib_update_entry(&m_sdk_oid, 1, 10, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_11) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 11, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_12) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 12, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_13) == -1   
                        || mib_update_entry(&m_sdk_oid, 1, 13, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_14) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 14, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_15) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 15, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_16) == -1   
                        || mib_update_entry(&m_sdk_oid, 1, 16, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_17) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 17, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_18) == -1
                        || mib_update_entry(&m_sdk_oid, 1, 18, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_19) == -1   
                        || mib_update_entry(&m_sdk_oid, 1, 19, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.dry_contact_20) == -1
			|| mib_update_entry(&m_sdk_oid, 2, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.sdk_temp) == -1
			|| mib_update_entry(&m_sdk_oid, 3, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.sdk_hw) == -1
			|| mib_update_entry(&m_sdk_oid, 4, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.sdk_sw) == -1
			|| mib_update_entry(&m_sdk_oid, 5, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.sdk_relay) == -1
			|| mib_update_entry(&m_sdk_oid, 6, 0, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.optical_relay_1) == -1
			|| mib_update_entry(&m_sdk_oid, 6, 1, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.optical_relay_2) == -1
			|| mib_update_entry(&m_sdk_oid, 6, 2, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.optical_relay_3) == -1
			|| mib_update_entry(&m_sdk_oid, 6, 3, &pos, BER_TYPE_INTEGER, (const void *)u.sdkinfo.optical_relay_4) == -1) {
			return -1;
		}
	}


	return 0;
}

int mib_find(const oid_t *oid)
{
	int pos;

	/* Find the OID in the MIB that is exactly the given one or a subid */
	for (pos = 0; pos < g_mib_length; pos++) {
		if (g_mib[pos].oid.subid_list_length >= oid->subid_list_length
			&& !memcmp(g_mib[pos].oid.subid_list, oid->subid_list,
				oid->subid_list_length * sizeof (oid->subid_list[0]))) {
			break;
		}
	}

	return pos;
}

int mib_findnext(const oid_t *oid)
{
	int pos;

	/* Find the OID in the MIB that is the one after the given one */
	for (pos = 0; pos < g_mib_length; pos++) {
		if (oid_cmp(&g_mib[pos].oid, oid) > 0) {
			break;
		}
	}

	return pos;
}



/* vim: ts=4 sts=4 sw=4 nowrap
 */
