/*-
 * Copyright (c) 2014-2019 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

static int __ckpt_last(WT_SESSION_IMPL *, const char *, WT_CKPT *);
static int __ckpt_last_name(WT_SESSION_IMPL *, const char *, const char **);
static int __ckpt_load(WT_SESSION_IMPL *,
		WT_CONFIG_ITEM *, WT_CONFIG_ITEM *, WT_CKPT *);
static int __ckpt_named(
		WT_SESSION_IMPL *, const char *, const char *, WT_CKPT *);
static int __ckpt_set(WT_SESSION_IMPL *, const char *, const char *);
static int __ckpt_version_chk(WT_SESSION_IMPL *, const char *, const char *);

/*
 * __wt_meta_checkpoint --
 *	Return a file's checkpoint information.
 */
int
__wt_meta_checkpoint(WT_SESSION_IMPL *session,
    const char *fname, const char *checkpoint, WT_CKPT *ckpt)
{
	WT_DECL_RET;
	char *config;

	config = NULL;

	/* Clear the returned information. */
	memset(ckpt, 0, sizeof(*ckpt));

	/* Retrieve the metadata entry for the file. */
	WT_ERR(__wt_metadata_search(session, fname, &config));

	/* Check the major/minor version numbers. */
	WT_ERR(__ckpt_version_chk(session, fname, config));

	/*
	 * Retrieve the named checkpoint or the last checkpoint.
	 *
	 * If we don't find a named checkpoint, we're done, they're read-only.
	 * If we don't find a default checkpoint, it's creation, return "no
	 * data" and let our caller handle it.
	 */
	if (checkpoint == NULL) {
		if ((ret = __ckpt_last(session, config, ckpt)) == WT_NOTFOUND) {
			ret = 0;
			ckpt->addr.data = ckpt->raw.data = NULL;
			ckpt->addr.size = ckpt->raw.size = 0;
		}
	} else
		WT_ERR(__ckpt_named(session, checkpoint, config, ckpt));

err:	__wt_free(session, config);
	return (ret);
}

/*
 * __wt_meta_checkpoint_last_name --
 *	Return the last unnamed checkpoint's name.
 */
int
__wt_meta_checkpoint_last_name(
    WT_SESSION_IMPL *session, const char *fname, const char **namep)
{
	WT_DECL_RET;
	char *config;

	config = NULL;

	/* Retrieve the metadata entry for the file. */
	WT_ERR(__wt_metadata_search(session, fname, &config));

	/* Check the major/minor version numbers. */
	WT_ERR(__ckpt_version_chk(session, fname, config));

	/* Retrieve the name of the last unnamed checkpoint. */
	WT_ERR(__ckpt_last_name(session, config, namep));

err:	__wt_free(session, config);
	return (ret);
}

/*
 * __wt_meta_checkpoint_clear --
 *	Clear a file's checkpoint.
 */
int
__wt_meta_checkpoint_clear(WT_SESSION_IMPL *session, const char *fname)
{
	/*
	 * If we are unrolling a failed create, we may have already removed the
	 * metadata entry.  If no entry is found to update and we're trying to
	 * clear the checkpoint, just ignore it.
	 */
	WT_RET_NOTFOUND_OK(__ckpt_set(session, fname, NULL));

	return (0);
}

/*
 * __ckpt_set --
 *	Set a file's checkpoint.
 */
static int
__ckpt_set(WT_SESSION_IMPL *session, const char *fname, const char *v)
{
	WT_DECL_RET;
	const char *cfg[3];
	char *config, *newcfg;

	config = newcfg = NULL;

	/* Retrieve the metadata for this file. */
	WT_ERR(__wt_metadata_search(session, fname, &config));

	/* Replace the checkpoint entry. */
	cfg[0] = config;
	cfg[1] = v == NULL ? "checkpoint=()" : v;
	cfg[2] = NULL;
	WT_ERR(__wt_config_collapse(session, cfg, &newcfg));
	WT_ERR(__wt_metadata_update(session, fname, newcfg));

err:	__wt_free(session, config);
	__wt_free(session, newcfg);
	return (ret);
}

/*
 * __ckpt_named --
 *	Return the information associated with a file's named checkpoint.
 */
static int
__ckpt_named(WT_SESSION_IMPL *session,
    const char *checkpoint, const char *config, WT_CKPT *ckpt)
{
	WT_CONFIG ckptconf;
	WT_CONFIG_ITEM k, v;

	WT_RET(__wt_config_getones(session, config, "checkpoint", &v));
	__wt_config_subinit(session, &ckptconf, &v);

	/*
	 * Take the first match: there should never be more than a single
	 * checkpoint of any name.
	 */
	while (__wt_config_next(&ckptconf, &k, &v) == 0)
		if (WT_STRING_MATCH(checkpoint, k.str, k.len))
			return (__ckpt_load(session, &k, &v, ckpt));

	return (WT_NOTFOUND);
}

/*
 * __ckpt_last --
 *	Return the information associated with the file's last checkpoint.
 */
static int
__ckpt_last(WT_SESSION_IMPL *session, const char *config, WT_CKPT *ckpt)
{
	WT_CONFIG ckptconf;
	WT_CONFIG_ITEM a, k, v;
	int64_t found;

	WT_RET(__wt_config_getones(session, config, "checkpoint", &v));
	__wt_config_subinit(session, &ckptconf, &v);
	for (found = 0; __wt_config_next(&ckptconf, &k, &v) == 0;) {
		/* Ignore checkpoints before the ones we've already seen. */
		WT_RET(__wt_config_subgets(session, &v, "order", &a));
		if (found) {
			if (a.val < found)
				continue;
			__wt_meta_checkpoint_free(session, ckpt);
		}
		found = a.val;
		WT_RET(__ckpt_load(session, &k, &v, ckpt));
	}

	return (found ? 0 : WT_NOTFOUND);
}

/*
 * __ckpt_last_name --
 *	Return the name associated with the file's last unnamed checkpoint.
 */
static int
__ckpt_last_name(
    WT_SESSION_IMPL *session, const char *config, const char **namep)
{
	WT_CONFIG ckptconf;
	WT_CONFIG_ITEM a, k, v;
	WT_DECL_RET;
	int64_t found;

	*namep = NULL;

	WT_ERR(__wt_config_getones(session, config, "checkpoint", &v));
	__wt_config_subinit(session, &ckptconf, &v);
	for (found = 0; __wt_config_next(&ckptconf, &k, &v) == 0;) {
		/*
		 * We only care about unnamed checkpoints; applications may not
		 * use any matching prefix as a checkpoint name, the comparison
		 * is pretty simple.
		 */
		if (k.len < strlen(WT_CHECKPOINT) ||
		    strncmp(k.str, WT_CHECKPOINT, strlen(WT_CHECKPOINT)) != 0)
			continue;

		/* Ignore checkpoints before the ones we've already seen. */
		WT_ERR(__wt_config_subgets(session, &v, "order", &a));
		if (found && a.val < found)
			continue;

		__wt_free(session, *namep);
		WT_ERR(__wt_strndup(session, k.str, k.len, namep));
		found = a.val;
	}
	if (!found)
		ret = WT_NOTFOUND;

	if (0) {
err:		__wt_free(session, *namep);
	}
	return (ret);
}

/*
 * __wt_meta_block_metadata --
 *	Build a version of the file's metadata for the block manager to store.
 */
int
__wt_meta_block_metadata(
    WT_SESSION_IMPL *session, const char *config, WT_CKPT *ckpt)
{
	WT_CONFIG_ITEM cval;
	WT_DECL_ITEM(a);
	WT_DECL_ITEM(b);
	WT_DECL_RET;
	WT_KEYED_ENCRYPTOR *kencryptor;
	size_t encrypt_size, metadata_len;
	const char *metadata, *filecfg[] = {
	    WT_CONFIG_BASE(session, file_meta), NULL, NULL };
	char *min_config;

	min_config = NULL;
	WT_ERR(__wt_scr_alloc(session, 0, &a));
	WT_ERR(__wt_scr_alloc(session, 0, &b));

	/*
	 * The metadata has to be encrypted because it contains private data
	 * (for example, column names). We pass the block manager text that
	 * describes the metadata (the encryption information), and the
	 * possibly encrypted metadata encoded as a hexadecimal string.
	 * configuration string.
	 *
	 * Get a minimal configuration string, just the non-default entries.
	 */
	WT_ERR(__wt_config_discard_defaults(
	    session, filecfg, config, &min_config));

	/* Fill out the configuration array for normal retrieval. */
	filecfg[1] = config;

	/*
	 * Find out if this file is encrypted. If encrypting, encrypt and encode
	 * the minimal configuration.
	 */
	WT_ERR(__wt_btree_config_encryptor(session, filecfg, &kencryptor));
	if (kencryptor == NULL) {
		metadata = min_config;
		metadata_len = strlen(min_config);
	} else {
		__wt_buf_set(session, a, min_config, strlen(min_config));
		__wt_encrypt_size(session, kencryptor, a->size, &encrypt_size);
		WT_ERR(__wt_buf_grow(session, b, encrypt_size));
		WT_ERR(__wt_encrypt(session, kencryptor, 0, a, b));
		WT_ERR(__wt_buf_grow(session, a, b->size * 2));
		__wt_fill_hex(b->mem, b->size, a->mem, a->memsize, &a->size);

		metadata = a->data;
		metadata_len = a->size;
	}

	/*
	 * Get a copy of the encryption information and flag if we're doing
	 * encryption. The latter isn't necessary, but it makes it easier to
	 * diagnose issues during the load.
	 */
	WT_ERR(__wt_config_gets(session, filecfg, "encryption", &cval));
	WT_ERR(__wt_buf_fmt(session, b,
	    "encryption=%.*s,"
	    "block_metadata_encrypted=%s,block_metadata=[%.*s]",
	    (int)cval.len, cval.str, kencryptor == NULL ? "false" : "true",
	    (int)metadata_len, metadata));
	WT_ERR(__wt_strndup(session, b->data, b->size, &ckpt->block_metadata));

err:
	__wt_free(session, min_config);
	__wt_scr_free(session, &a);
	__wt_scr_free(session, &b);
	return (ret);
}

/*
 * __ckpt_compare_order --
 *	Qsort comparison routine for the checkpoint list.
 */
static int WT_CDECL
__ckpt_compare_order(const void *a, const void *b)
{
	WT_CKPT *ackpt, *bckpt;

	ackpt = (WT_CKPT *)a;
	bckpt = (WT_CKPT *)b;

	return (ackpt->order > bckpt->order ? 1 : -1);
}

/*
 * __wt_meta_ckptlist_get --
 *	Load all available checkpoint information for a file.
 */
int
__wt_meta_ckptlist_get(WT_SESSION_IMPL *session,
    const char *fname, bool update, WT_CKPT **ckptbasep)
{
	WT_CKPT *ckpt, *ckptbase;
	WT_CONFIG ckptconf;
	WT_CONFIG_ITEM k, v;
	WT_DECL_RET;
	size_t allocated, slot;
	int64_t maxorder;
	char *config;

	*ckptbasep = NULL;

	ckptbase = NULL;
	allocated = slot = 0;
	config = NULL;

	/* Retrieve the metadata information for the file. */
	WT_RET(__wt_metadata_search(session, fname, &config));

	/* Load any existing checkpoints into the array. */
	if ((ret =
	    __wt_config_getones(session, config, "checkpoint", &v)) == 0) {
		__wt_config_subinit(session, &ckptconf, &v);
		for (; __wt_config_next(&ckptconf, &k, &v) == 0; ++slot) {
			WT_ERR(__wt_realloc_def(
			    session, &allocated, slot + 1, &ckptbase));
			ckpt = &ckptbase[slot];

			WT_ERR(__ckpt_load(session, &k, &v, ckpt));
		}
	}
	WT_ERR_NOTFOUND_OK(ret);
	if (!update && slot == 0)
		WT_ERR(WT_NOTFOUND);

	/* Sort in creation-order. */
	__wt_qsort(ckptbase, slot, sizeof(WT_CKPT), __ckpt_compare_order);

	if (update) {
		/*
		 * Allocate an extra slot for a new value, plus a slot to mark
		 * mark the end.
		 *
		 * This isn't clean, but there's necessary cooperation between
		 * the schema layer (that maintains the list of checkpoints),
		 * the btree layer (that knows when the root page is written,
		 * creating a new checkpoint), and the block manager (which
		 * actually creates the checkpoint). All of that cooperation is
		 * handled in the array of checkpoint structures referenced from
		 * the WT_BTREE structure.
		 */
		WT_ERR(__wt_realloc_def(
		    session, &allocated, slot + 2, &ckptbase));

		/* The caller may be adding a value, initialize it. */
		maxorder = 0;
		WT_CKPT_FOREACH(ckptbase, ckpt)
			if (ckpt->order > maxorder)
				maxorder = ckpt->order;
		ckpt->order = maxorder + 1;
		__wt_seconds(session, &ckpt->sec);

		WT_ERR(__wt_meta_block_metadata(session, config, ckpt));

		F_SET(ckpt, WT_CKPT_ADD);
	}

	/* Return the array to our caller. */
	*ckptbasep = ckptbase;

	if (0) {
err:		__wt_meta_ckptlist_free(session, &ckptbase);
	}
	__wt_free(session, config);

	return (ret);
}

/*
 * __ckpt_load --
 *	Load a single checkpoint's information into a WT_CKPT structure.
 */
static int
__ckpt_load(WT_SESSION_IMPL *session,
    WT_CONFIG_ITEM *k, WT_CONFIG_ITEM *v, WT_CKPT *ckpt)
{
	WT_CONFIG_ITEM a;
	WT_DECL_RET;
	char timebuf[64];

	/*
	 * Copy the name, address (raw and hex), order and time into the slot.
	 * If there's no address, it's a fake.
	 */
	WT_RET(__wt_strndup(session, k->str, k->len, &ckpt->name));

	WT_RET(__wt_config_subgets(session, v, "addr", &a));
	WT_RET(__wt_buf_set(session, &ckpt->addr, a.str, a.len));
	if (a.len == 0)
		F_SET(ckpt, WT_CKPT_FAKE);
	else
		WT_RET(__wt_nhex_to_raw(session, a.str, a.len, &ckpt->raw));

	WT_RET(__wt_config_subgets(session, v, "order", &a));
	if (a.len == 0)
		goto format;
	ckpt->order = a.val;

	WT_RET(__wt_config_subgets(session, v, "time", &a));
	if (a.len == 0 || a.len > sizeof(timebuf) - 1)
		goto format;
	memcpy(timebuf, a.str, a.len);
	timebuf[a.len] = '\0';
	/* NOLINTNEXTLINE(cert-err34-c) */
	if (sscanf(timebuf, "%" SCNu64, &ckpt->sec) != 1)
		goto format;

	WT_RET(__wt_config_subgets(session, v, "size", &a));
	ckpt->size = (uint64_t)a.val;

	/* Default to durability. */
	ret = __wt_config_subgets(session, v, "newest_durable_ts", &a);
	WT_RET_NOTFOUND_OK(ret);
	ckpt->newest_durable_ts =
	    ret == WT_NOTFOUND || a.len == 0 ? WT_TS_NONE : (uint64_t)a.val;
	ret = __wt_config_subgets(session, v, "oldest_start_ts", &a);
	WT_RET_NOTFOUND_OK(ret);
	ckpt->oldest_start_ts =
	    ret == WT_NOTFOUND || a.len == 0 ? WT_TS_NONE : (uint64_t)a.val;
	ret = __wt_config_subgets(session, v, "oldest_start_txn", &a);
	WT_RET_NOTFOUND_OK(ret);
	ckpt->oldest_start_txn =
	    ret == WT_NOTFOUND || a.len == 0 ? WT_TXN_NONE : (uint64_t)a.val;
	ret = __wt_config_subgets(session, v, "newest_stop_ts", &a);
	WT_RET_NOTFOUND_OK(ret);
	ckpt->newest_stop_ts =
	    ret == WT_NOTFOUND || a.len == 0 ? WT_TS_MAX : (uint64_t)a.val;
	ret = __wt_config_subgets(session, v, "newest_stop_txn", &a);
	WT_RET_NOTFOUND_OK(ret);
	ckpt->newest_stop_txn =
	    ret == WT_NOTFOUND || a.len == 0 ? WT_TXN_MAX : (uint64_t)a.val;
	__wt_check_addr_validity(session,
	    ckpt->oldest_start_ts, ckpt->oldest_start_txn,
	    ckpt->newest_stop_ts, ckpt->newest_stop_txn);

	WT_RET(__wt_config_subgets(session, v, "write_gen", &a));
	if (a.len == 0)
		goto format;
	ckpt->write_gen = (uint64_t)a.val;

	return (0);

format:
	WT_RET_MSG(session, WT_ERROR, "corrupted checkpoint list");
}

/*
 * __wt_metadata_set_base_write_gen --
 *	Set the connection's base write generation.
 */
int
__wt_metadata_set_base_write_gen(WT_SESSION_IMPL *session)
{
	WT_CKPT ckpt;

	WT_RET(__wt_meta_checkpoint(session, WT_METAFILE_URI, NULL, &ckpt));

	/*
	 * We track the maximum page generation we've ever seen, and I'm not
	 * interested in debugging off-by-ones.
	 */
	S2C(session)->base_write_gen = ckpt.write_gen + 1;

	__wt_meta_checkpoint_free(session, &ckpt);

	return (0);
}

/*
 * __ckptlist_review_write_gen --
 *	Review the checkpoint's write generation.
 */
static void
__ckptlist_review_write_gen(WT_SESSION_IMPL *session, WT_CKPT *ckpt)
{
	uint64_t v;

	/*
	 * Every page written in a given wiredtiger_open() session needs to be
	 * in a single "generation", it's how we know to ignore transactional
	 * information found on pages written in previous generations. We make
	 * this work by writing the maximum write generation we've ever seen
	 * as the write-generation of the metadata file's checkpoint. When
	 * wiredtiger_open() is called, we copy that write generation into the
	 * connection's name space as the base write generation value. Then,
	 * whenever we open a file, if the file's write generation is less than
	 * the base value, we update the file's write generation so all writes
	 * will appear after the base value, and we ignore transactions on pages
	 * where the write generation is less than the base value.
	 *
	 * At every checkpoint, if the file's checkpoint write generation is
	 * larger than the connection's maximum write generation, update the
	 * connection.
	 */
	do {
		WT_ORDERED_READ(v, S2C(session)->max_write_gen);
	} while (ckpt->write_gen > v && !__wt_atomic_cas64(
	    &S2C(session)->max_write_gen, v, ckpt->write_gen));

	/*
	 * If checkpointing the metadata file, update its write generation to
	 * be the maximum we've seen.
	 */
	if (session->dhandle != NULL &&
	    WT_IS_METADATA(session->dhandle) && ckpt->write_gen < v)
		ckpt->write_gen = v;
}

/*
 * __wt_meta_ckptlist_to_meta --
 *	Convert a checkpoint list into its metadata representation.
 */
int
__wt_meta_ckptlist_to_meta(
    WT_SESSION_IMPL *session, WT_CKPT *ckptbase, WT_ITEM *buf)
{
	WT_CKPT *ckpt;
	const char *sep;

	sep = "";
	WT_RET(__wt_buf_fmt(session, buf, "checkpoint=("));
	WT_CKPT_FOREACH(ckptbase, ckpt) {
		/* Skip deleted checkpoints. */
		if (F_ISSET(ckpt, WT_CKPT_DELETE))
			continue;

		if (F_ISSET(ckpt, WT_CKPT_ADD | WT_CKPT_UPDATE)) {
			/*
			 * We fake checkpoints for handles in the middle of a
			 * bulk load.  If there is a checkpoint, convert the
			 * raw cookie to a hex string.
			 */
			if (ckpt->raw.size == 0)
				ckpt->addr.size = 0;
			else
				WT_RET(__wt_raw_to_hex(session,
				    ckpt->raw.data,
				    ckpt->raw.size, &ckpt->addr));
		}

		__wt_check_addr_validity(session,
		    ckpt->oldest_start_ts, ckpt->oldest_start_txn,
		    ckpt->newest_stop_ts, ckpt->newest_stop_txn);

		WT_RET(__wt_buf_catfmt(session, buf, "%s%s", sep, ckpt->name));
		sep = ",";

		if (strcmp(ckpt->name, WT_CHECKPOINT) == 0)
			WT_RET(__wt_buf_catfmt(session, buf,
			    ".%" PRId64, ckpt->order));

		/*
		 * Use PRId64 formats: WiredTiger's configuration code handles
		 * signed 8B values.
		 */
		WT_RET(__wt_buf_catfmt(session, buf,
		    "=(addr=\"%.*s\",order=%" PRId64
		    ",time=%" PRIu64
		    ",size=%" PRId64
		    ",newest_durable_ts=%" PRId64
		    ",oldest_start_ts=%" PRId64
		    ",oldest_start_txn=%" PRId64
		    ",newest_stop_ts=%" PRId64
		    ",newest_stop_txn=%" PRId64
		    ",write_gen=%" PRId64 ")",
		    (int)ckpt->addr.size, (char *)ckpt->addr.data,
		    ckpt->order,
		    ckpt->sec,
		    (int64_t)ckpt->size,
		    (int64_t)ckpt->newest_durable_ts,
		    (int64_t)ckpt->oldest_start_ts,
		    (int64_t)ckpt->oldest_start_txn,
		    (int64_t)ckpt->newest_stop_ts,
		    (int64_t)ckpt->newest_stop_txn,
		    (int64_t)ckpt->write_gen));
	}
	WT_RET(__wt_buf_catfmt(session, buf, ")"));

	return (0);
}

/*
 * __wt_meta_ckptlist_set --
 *	Set a file's checkpoint value from the WT_CKPT list.
 */
int
__wt_meta_ckptlist_set(WT_SESSION_IMPL *session,
    const char *fname, WT_CKPT *ckptbase, WT_LSN *ckptlsn)
{
	WT_CKPT *ckpt;
	WT_DECL_ITEM(buf);
	WT_DECL_RET;

	WT_RET(__wt_scr_alloc(session, 1024, &buf));

	WT_ERR(__wt_meta_ckptlist_to_meta(session, ckptbase, buf));

	if (ckptlsn != NULL)
		WT_ERR(__wt_buf_catfmt(session, buf,
		    ",checkpoint_lsn=(%" PRIu32 ",%" PRIuMAX ")",
		    ckptlsn->l.file, (uintmax_t)ckptlsn->l.offset));

	WT_ERR(__ckpt_set(session, fname, buf->mem));

	/* Review the checkpoint's write generation. */
	WT_CKPT_FOREACH(ckptbase, ckpt)
		__ckptlist_review_write_gen(session, ckpt);

err:	__wt_scr_free(session, &buf);
	return (ret);
}

/*
 * __wt_meta_ckptlist_free --
 *	Discard the checkpoint array.
 */
void
__wt_meta_ckptlist_free(WT_SESSION_IMPL *session, WT_CKPT **ckptbasep)
{
	WT_CKPT *ckpt, *ckptbase;

	if ((ckptbase = *ckptbasep) == NULL)
		return;

	WT_CKPT_FOREACH(ckptbase, ckpt)
		__wt_meta_checkpoint_free(session, ckpt);
	__wt_free(session, *ckptbasep);
}

/*
 * __wt_meta_checkpoint_free --
 *	Clean up a single checkpoint structure.
 */
void
__wt_meta_checkpoint_free(WT_SESSION_IMPL *session, WT_CKPT *ckpt)
{
	if (ckpt == NULL)
		return;

	__wt_free(session, ckpt->name);
	__wt_free(session, ckpt->block_metadata);
	__wt_free(session, ckpt->block_checkpoint);
	__wt_buf_free(session, &ckpt->addr);
	__wt_buf_free(session, &ckpt->raw);
	__wt_free(session, ckpt->bpriv);

	WT_CLEAR(*ckpt);		/* Clear to prepare for re-use. */
}

/*
 * __wt_meta_sysinfo_set --
 *	Set the system information in the metadata.
 */
int
__wt_meta_sysinfo_set(WT_SESSION_IMPL *session)
{
	WT_DECL_ITEM(buf);
	WT_DECL_RET;
	char hex_timestamp[2 * sizeof(wt_timestamp_t) + 2];

	WT_ERR(__wt_scr_alloc(session, 0, &buf));
	hex_timestamp[0] = '0';
	hex_timestamp[1] = '\0';

	/*
	 * We need to record the timestamp of the checkpoint in the metadata.
	 * The timestamp value is set at a higher level, either in checkpoint
	 * or in recovery.
	 */
	__wt_timestamp_to_hex_string(
	    S2C(session)->txn_global.meta_ckpt_timestamp, hex_timestamp);

	/*
	 * Don't leave a zero entry in the metadata: remove it.  This avoids
	 * downgrade issues if the metadata is opened with an older version of
	 * WiredTiger that does not understand the new entry.
	 */
	if (strcmp(hex_timestamp, "0") == 0)
		WT_ERR_NOTFOUND_OK(
		    __wt_metadata_remove(session, WT_SYSTEM_CKPT_URI));
	else {
		WT_ERR(__wt_buf_catfmt(session, buf,
		    "checkpoint_timestamp=\"%s\"", hex_timestamp));
		WT_ERR(__wt_metadata_update(
		    session, WT_SYSTEM_CKPT_URI, buf->data));
	}

err:	__wt_scr_free(session, &buf);
	return (ret);
}

/*
 * __ckpt_version_chk --
 *	Check the version major/minor numbers.
 */
static int
__ckpt_version_chk(
    WT_SESSION_IMPL *session, const char *fname, const char *config)
{
	WT_CONFIG_ITEM a, v;
	int majorv, minorv;

	WT_RET(__wt_config_getones(session, config, "version", &v));
	WT_RET(__wt_config_subgets(session, &v, "major", &a));
	majorv = (int)a.val;
	WT_RET(__wt_config_subgets(session, &v, "minor", &a));
	minorv = (int)a.val;

	if (majorv < WT_BTREE_MAJOR_VERSION_MIN ||
	    majorv > WT_BTREE_MAJOR_VERSION_MAX ||
	    (majorv == WT_BTREE_MAJOR_VERSION_MIN &&
	    minorv < WT_BTREE_MINOR_VERSION_MIN) ||
	    (majorv == WT_BTREE_MAJOR_VERSION_MAX &&
	    minorv > WT_BTREE_MINOR_VERSION_MAX))
		WT_RET_MSG(session, EACCES,
		    "%s is an unsupported WiredTiger source file version %d.%d"
		    "; this WiredTiger build only supports versions from %d.%d "
		    "to %d.%d",
		    fname,
		    majorv, minorv,
		    WT_BTREE_MAJOR_VERSION_MIN,
		    WT_BTREE_MINOR_VERSION_MIN,
		    WT_BTREE_MAJOR_VERSION_MAX,
		    WT_BTREE_MINOR_VERSION_MAX);
	return (0);
}
