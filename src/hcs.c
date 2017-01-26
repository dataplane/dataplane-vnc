/*
 * Hash Chaining Scheme
 */

#include "fvncd.h"
#include <stdlib.h>
#include <time.h>
#include "hcs.h"
#include "rfb.h"

int g_hcs_startup_flag = FALSE;
BUCKET g_hcs_table[TABLE_SIZE];

void hcs_init(void) {
	register int i;

	for (i = 0; i < TABLE_SIZE; i++) {
		g_hcs_table[i].head = EMPTY;
		g_hcs_table[i].tail = EMPTY;
	}

	g_hcs_startup_flag = TRUE;

	return;
}

/* search in order of left->right */
inline SLOT * hcs_search(const keysa_t key) {
	SLOT *_slot;

	if ((_slot = g_hcs_table[HASH(key)].head) != EMPTY) {
		do {
			if (_slot->key == key)
				return _slot;
		}
		while ((_slot = _slot->next));
	}

	return EMPTY;
}

/* intert in front (create new slot) */
inline SLOT * hcs_insert(const keysa_t key) {
	if (hcs_search(key) == EMPTY) {
		SLOT *_slot;
		SLOT *_next;
		BUCKET *_bucket = (BUCKET *)&(g_hcs_table[HASH(key)]);

		if ((_slot = (SLOT *)calloc(1, sizeof(SLOT))) == EMPTY) {
			warning("cannot allocate memory for new slot");
			return EMPTY;
		}

		_next = _bucket->head;
		_bucket->head = _slot;
		if (_bucket->tail == EMPTY)
			_bucket->tail = _slot;
		if (_next != EMPTY)
			_next->prev = _slot;

		/* init */
		_slot->key = key;
		_slot->prev = EMPTY;	/* first node */
		_slot->next = _next;

		/* init adj */
		_slot->step = RFB_STEP_INIT;
		_slot->version = -1;
		_slot->sectype = -1;

		/* update */
		hcs_update(_slot);

		return _slot;
	}

	warning("already inserted slot#%d", key);
	return EMPTY;
}

/* update access information of the slot */
inline void hcs_update(SLOT *slot) {
	time_t _ts;

	time(&_ts);
	slot->time = _ts;

	return;
}

/* delete the closed slot */
inline void hcs_delete(SLOT *slot) {
	BUCKET *_bucket;

	if (slot == EMPTY) {
		warning("cannot delete the slot of null pointer");
		return;
	}

	_bucket = (BUCKET *)&(g_hcs_table[HASH(slot->key)]);

	/* link of prev node */
	if (slot->prev == EMPTY)    /* in case of the first node */
		_bucket->head = slot->next;
	else
		slot->prev->next = slot->next;

	/* link of next node */
	if (slot->next == EMPTY)    /* in case of the last node */
		_bucket->tail = slot->prev;
	else
		slot->next->prev = slot->prev;

	FREE(slot);

	return;
}

/* free slots -> free buckets */
void hcs_final(void) {
	if (g_hcs_startup_flag) {
		SLOT *_slot, *_next;
		register int i;

		for (i = 0; i < TABLE_SIZE; i++) {
			if ((_next = g_hcs_table[i].head) != EMPTY) {
				do {
					_slot = _next;
					_next = _next->next;

					hcs_delete(_slot);
				}
				while (_next != EMPTY);
			}
			g_hcs_table[i].tail = EMPTY;
		}

		g_hcs_startup_flag = FALSE;
	}

	return;
}
