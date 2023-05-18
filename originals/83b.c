static int phar_parse_pharfile(php_stream *fp, char *fname, int fname_len, char *alias, int alias_len, long halt_offset, phar_archive_data** pphar, php_uint32 compression, char **error TSRMLS_DC) /* {{{ */
{
	char b32[4], *buffer, *endbuffer, *savebuf;
	phar_archive_data *mydata = NULL;
	phar_entry_info entry;
	php_uint32 manifest_len, manifest_count, manifest_flags, manifest_index, tmp_len, sig_flags;
	php_uint16 manifest_ver;
	php_uint32 len;
	long offset;
	int sig_len, register_alias = 0, temp_alias = 0;
	char *signature = NULL;

	if (pphar) {
		*pphar = NULL;
	}

	if (error) {
		*error = NULL;
	}

	/* check for ?>\n and increment accordingly */
	if (-1 == php_stream_seek(fp, halt_offset, SEEK_SET)) {
		MAPPHAR_ALLOC_FAIL("cannot seek to __HALT_COMPILER(); location in phar \"%s\"")
	}

	buffer = b32;

	if (3 != php_stream_read(fp, buffer, 3)) {
		MAPPHAR_ALLOC_FAIL("internal corruption of phar \"%s\" (truncated manifest at stub end)")
	}

	if ((*buffer == ' ' || *buffer == '\n') && *(buffer + 1) == '?' && *(buffer + 2) == '>') {
		int nextchar;
		halt_offset += 3;
		if (EOF == (nextchar = php_stream_getc(fp))) {
			MAPPHAR_ALLOC_FAIL("internal corruption of phar \"%s\" (truncated manifest at stub end)")
		}

		if ((char) nextchar == '\r') {
			/* if we have an \r we require an \n as well */
			if (EOF == (nextchar = php_stream_getc(fp)) || (char)nextchar != '\n') {
				MAPPHAR_ALLOC_FAIL("internal corruption of phar \"%s\" (truncated manifest at stub end)")
			}
			++halt_offset;
		}

		if ((char) nextchar == '\n') {
			++halt_offset;
		}
	}

	/* make sure we are at the right location to read the manifest */
	if (-1 == php_stream_seek(fp, halt_offset, SEEK_SET)) {
		MAPPHAR_ALLOC_FAIL("cannot seek to __HALT_COMPILER(); location in phar \"%s\"")
	}

	/* read in manifest */
	buffer = b32;

	if (4 != php_stream_read(fp, buffer, 4)) {
		MAPPHAR_ALLOC_FAIL("internal corruption of phar \"%s\" (truncated manifest at manifest length)")
	}

	PHAR_GET_32(buffer, manifest_len);

	if (manifest_len > 1048576 * 100) {
		/* prevent serious memory issues by limiting manifest to at most 100 MB in length */
		MAPPHAR_ALLOC_FAIL("manifest cannot be larger than 100 MB in phar \"%s\"")
	}

	buffer = (char *)emalloc(manifest_len);
	savebuf = buffer;
	endbuffer = buffer + manifest_len;

	if (manifest_len < 10 || manifest_len != php_stream_read(fp, buffer, manifest_len)) {
		MAPPHAR_FAIL("internal corruption of phar \"%s\" (truncated manifest header)")
	}

	/* extract the number of entries */
	PHAR_GET_32(buffer, manifest_count);

	if (manifest_count == 0) {
		MAPPHAR_FAIL("in phar \"%s\", manifest claims to have zero entries.  Phars must have at least 1 entry");
	}

	/* extract API version, lowest nibble currently unused */
	manifest_ver = (((unsigned char)buffer[0]) << 8)
				 + ((unsigned char)buffer[1]);
	buffer += 2;

	if ((manifest_ver & PHAR_API_VER_MASK) < PHAR_API_MIN_READ) {
		efree(savebuf);
		php_stream_close(fp);
		if (error) {
			spprintf(error, 0, "phar \"%s\" is API version %1.u.%1.u.%1.u, and cannot be processed", fname, manifest_ver >> 12, (manifest_ver >> 8) & 0xF, (manifest_ver >> 4) & 0x0F);
		}
		return FAILURE;
	}

	PHAR_GET_32(buffer, manifest_flags);

	manifest_flags &= ~PHAR_HDR_COMPRESSION_MASK;
	manifest_flags &= ~PHAR_FILE_COMPRESSION_MASK;
	/* remember whether this entire phar was compressed with gz/bzip2 */
	manifest_flags |= compression;

	/* The lowest nibble contains the phar wide flags. The compression flags can */
	/* be ignored on reading because it is being generated anyways. */
	if (manifest_flags & PHAR_HDR_SIGNATURE) {
		char sig_buf[8], *sig_ptr = sig_buf;
		off_t read_len;
		size_t end_of_phar;

		if (-1 == php_stream_seek(fp, -8, SEEK_END)
		|| (read_len = php_stream_tell(fp)) < 20
		|| 8 != php_stream_read(fp, sig_buf, 8)
		|| memcmp(sig_buf+4, "GBMB", 4)) {
			efree(savebuf);
			php_stream_close(fp);
			if (error) {
				spprintf(error, 0, "phar \"%s\" has a broken signature", fname);
			}
			return FAILURE;
		}

		PHAR_GET_32(sig_ptr, sig_flags);

		switch(sig_flags) {
			case PHAR_SIG_OPENSSL: {
				php_uint32 signature_len;
				char *sig;
				off_t whence;

				/* we store the signature followed by the signature length */
				if (-1 == php_stream_seek(fp, -12, SEEK_CUR)
				|| 4 != php_stream_read(fp, sig_buf, 4)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" openssl signature length could not be read", fname);
					}
					return FAILURE;
				}

				sig_ptr = sig_buf;
				PHAR_GET_32(sig_ptr, signature_len);
				sig = (char *) emalloc(signature_len);
				whence = signature_len + 4;
				whence = -whence;

				if (-1 == php_stream_seek(fp, whence, SEEK_CUR)
				|| !(end_of_phar = php_stream_tell(fp))
				|| signature_len != php_stream_read(fp, sig, signature_len)) {
					efree(savebuf);
					efree(sig);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" openssl signature could not be read", fname);
					}
					return FAILURE;
				}

				if (FAILURE == phar_verify_signature(fp, end_of_phar, PHAR_SIG_OPENSSL, sig, signature_len, fname, &signature, &sig_len, error TSRMLS_CC)) {
					efree(savebuf);
					efree(sig);
					php_stream_close(fp);
					if (error) {
						char *save = *error;
						spprintf(error, 0, "phar \"%s\" openssl signature could not be verified: %s", fname, *error);
						efree(save);
					}
					return FAILURE;
				}
				efree(sig);
			}
			break;
#if PHAR_HASH_OK
			case PHAR_SIG_SHA512: {
				unsigned char digest[64];

				php_stream_seek(fp, -(8 + 64), SEEK_END);
				read_len = php_stream_tell(fp);

				if (php_stream_read(fp, (char*)digest, sizeof(digest)) != sizeof(digest)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" has a broken signature", fname);
					}
					return FAILURE;
				}

				if (FAILURE == phar_verify_signature(fp, read_len, PHAR_SIG_SHA512, (char *)digest, 64, fname, &signature, &sig_len, error TSRMLS_CC)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						char *save = *error;
						spprintf(error, 0, "phar \"%s\" SHA512 signature could not be verified: %s", fname, *error);
						efree(save);
					}
					return FAILURE;
				}
				break;
			}
			case PHAR_SIG_SHA256: {
				unsigned char digest[32];

				php_stream_seek(fp, -(8 + 32), SEEK_END);
				read_len = php_stream_tell(fp);

				if (php_stream_read(fp, (char*)digest, sizeof(digest)) != sizeof(digest)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" has a broken signature", fname);
					}
					return FAILURE;
				}

				if (FAILURE == phar_verify_signature(fp, read_len, PHAR_SIG_SHA256, (char *)digest, 32, fname, &signature, &sig_len, error TSRMLS_CC)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						char *save = *error;
						spprintf(error, 0, "phar \"%s\" SHA256 signature could not be verified: %s", fname, *error);
						efree(save);
					}
					return FAILURE;
				}
				break;
			}
#else
			case PHAR_SIG_SHA512:
			case PHAR_SIG_SHA256:
				efree(savebuf);
				php_stream_close(fp);

				if (error) {
					spprintf(error, 0, "phar \"%s\" has a unsupported signature", fname);
				}
				return FAILURE;
#endif
			case PHAR_SIG_SHA1: {
				unsigned char digest[20];

				php_stream_seek(fp, -(8 + 20), SEEK_END);
				read_len = php_stream_tell(fp);

				if (php_stream_read(fp, (char*)digest, sizeof(digest)) != sizeof(digest)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" has a broken signature", fname);
					}
					return FAILURE;
				}

				if (FAILURE == phar_verify_signature(fp, read_len, PHAR_SIG_SHA1, (char *)digest, 20, fname, &signature, &sig_len, error TSRMLS_CC)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						char *save = *error;
						spprintf(error, 0, "phar \"%s\" SHA1 signature could not be verified: %s", fname, *error);
						efree(save);
					}
					return FAILURE;
				}
				break;
			}
			case PHAR_SIG_MD5: {
				unsigned char digest[16];

				php_stream_seek(fp, -(8 + 16), SEEK_END);
				read_len = php_stream_tell(fp);

				if (php_stream_read(fp, (char*)digest, sizeof(digest)) != sizeof(digest)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						spprintf(error, 0, "phar \"%s\" has a broken signature", fname);
					}
					return FAILURE;
				}

				if (FAILURE == phar_verify_signature(fp, read_len, PHAR_SIG_MD5, (char *)digest, 16, fname, &signature, &sig_len, error TSRMLS_CC)) {
					efree(savebuf);
					php_stream_close(fp);
					if (error) {
						char *save = *error;
						spprintf(error, 0, "phar \"%s\" MD5 signature could not be verified: %s", fname, *error);
						efree(save);
					}
					return FAILURE;
				}
				break;
			}
			default:
				efree(savebuf);
				php_stream_close(fp);

				if (error) {
					spprintf(error, 0, "phar \"%s\" has a broken or unsupported signature", fname);
				}
				return FAILURE;
		}
	} else if (PHAR_G(require_hash)) {
		efree(savebuf);
		php_stream_close(fp);

		if (error) {
			spprintf(error, 0, "phar \"%s\" does not have a signature", fname);
		}
		return FAILURE;
	} else {
		sig_flags = 0;
		sig_len = 0;
	}

	/* extract alias */
	PHAR_GET_32(buffer, tmp_len);

	if (buffer + tmp_len > endbuffer) {
		MAPPHAR_FAIL("internal corruption of phar \"%s\" (buffer overrun)");
	}

	if (manifest_len < 10 + tmp_len) {
		MAPPHAR_FAIL("internal corruption of phar \"%s\" (truncated manifest header)")
	}

	/* tmp_len = 0 says alias length is 0, which means the alias is not stored in the phar */
	if (tmp_len) {
		/* if the alias is stored we enforce it (implicit overrides explicit) */
		if (alias && alias_len && (alias_len != (int)tmp_len || strncmp(alias, buffer, tmp_len)))
		{
			buffer[tmp_len] = '\0';
			php_stream_close(fp);

			if (signature) {
				efree(signature);
			}

			if (error) {
				spprintf(error, 0, "cannot load phar \"%s\" with implicit alias \"%s\" under different alias \"%s\"", fname, buffer, alias);
			}

			efree(savebuf);
			return FAILURE;
		}

		alias_len = tmp_len;
		alias = buffer;
		buffer += tmp_len;
		register_alias = 1;
	} else if (!alias_len || !alias) {
		/* if we neither have an explicit nor an implicit alias, we use the filename */
		alias = NULL;
		alias_len = 0;
		register_alias = 0;
	} else if (alias_len) {
		register_alias = 1;
		temp_alias = 1;
	}

	/* we have 5 32-bit items plus 1 byte at least */
	if (manifest_count > ((manifest_len - 10 - tmp_len) / (5 * 4 + 1))) {
		/* prevent serious memory issues */
		MAPPHAR_FAIL("internal corruption of phar \"%s\" (too many manifest entries for size of manifest)")
	}

	mydata = pecalloc(1, sizeof(phar_archive_data), PHAR_G(persist));
	mydata->is_persistent = PHAR_G(persist);

	/* check whether we have meta data, zero check works regardless of byte order */
	PHAR_GET_32(buffer, len);
	if (mydata->is_persistent) {
		mydata->metadata_len = len;
		if(!len) {
			/* FIXME: not sure why this is needed but removing it breaks tests */
			PHAR_GET_32(buffer, len);
		}
	}
	if(len > endbuffer - buffer) {
		MAPPHAR_FAIL("internal corruption of phar \"%s\" (trying to read past buffer end)");
	}
	if (phar_parse_metadata(&buffer, &mydata->metadata, len TSRMLS_CC) == FAILURE) {
		MAPPHAR_FAIL("unable to read phar metadata in .phar file \"%s\"");
	}
	buffer += len;

	/* set up our manifest */
	zend_hash_init(&mydata->manifest, manifest_count,
		zend_get_hash_value, destroy_phar_manifest_entry, (zend_bool)mydata->is_persistent);
	zend_hash_init(&mydata->mounted_dirs, 5,
		zend_get_hash_value, NULL, (zend_bool)mydata->is_persistent);
	zend_hash_init(&mydata->virtual_dirs, manifest_count * 2,
		zend_get_hash_value, NULL, (zend_bool)mydata->is_persistent);
	mydata->fname = pestrndup(fname, fname_len, mydata->is_persistent);
#ifdef PHP_WIN32
	phar_unixify_path_separators(mydata->fname, fname_len);
#endif
	mydata->fname_len = fname_len;
	offset = halt_offset + manifest_len + 4;
	memset(&entry, 0, sizeof(phar_entry_info));
	entry.phar = mydata;
	entry.fp_type = PHAR_FP;
	entry.is_persistent = mydata->is_persistent;

	for (manifest_index = 0; manifest_index < manifest_count; ++manifest_index) {
		if (buffer + 4 > endbuffer) {
			MAPPHAR_FAIL("internal corruption of phar \"%s\" (truncated manifest entry)")
		}

		PHAR_GET_32(buffer, entry.filename_len);

		if (entry.filename_len == 0) {
			MAPPHAR_FAIL("zero-length filename encountered in phar \"%s\"");
		}

		if (entry.is_persistent) {
			entry.manifest_pos = manifest_index;
		}

		if (entry.filename_len + 20 > endbuffer - buffer) {
			MAPPHAR_FAIL("internal corruption of phar \"%s\" (truncated manifest entry)");
		}

		if ((manifest_ver & PHAR_API_VER_MASK) >= PHAR_API_MIN_DIR && buffer[entry.filename_len - 1] == '/') {
			entry.is_dir = 1;
		} else {
			entry.is_dir = 0;
		}

		phar_add_virtual_dirs(mydata, buffer, entry.filename_len TSRMLS_CC);
		entry.filename = pestrndup(buffer, entry.filename_len, entry.is_persistent);
		buffer += entry.filename_len;
		PHAR_GET_32(buffer, entry.uncompressed_filesize);
		PHAR_GET_32(buffer, entry.timestamp);

		if (offset == halt_offset + (int)manifest_len + 4) {
			mydata->min_timestamp = entry.timestamp;
			mydata->max_timestamp = entry.timestamp;
		} else {
			if (mydata->min_timestamp > entry.timestamp) {
				mydata->min_timestamp = entry.timestamp;
			} else if (mydata->max_timestamp < entry.timestamp) {
				mydata->max_timestamp = entry.timestamp;
			}
		}

		PHAR_GET_32(buffer, entry.compressed_filesize);
		PHAR_GET_32(buffer, entry.crc32);
		PHAR_GET_32(buffer, entry.flags);

		if (entry.is_dir) {
			entry.filename_len--;
			entry.flags |= PHAR_ENT_PERM_DEF_DIR;
		}

		PHAR_GET_32(buffer, len);
		if (entry.is_persistent) {
			entry.metadata_len = len;
		} else {
			entry.metadata_len = 0;
		}
		if (len > endbuffer - buffer) {
			pefree(entry.filename, entry.is_persistent);
			MAPPHAR_FAIL("internal corruption of phar \"%s\" (truncated manifest entry)");
		}
		if (phar_parse_metadata(&buffer, &entry.metadata, len TSRMLS_CC) == FAILURE) {
			pefree(entry.filename, entry.is_persistent);
			MAPPHAR_FAIL("unable to read file metadata in .phar file \"%s\"");
		}
		buffer += len;

		entry.offset = entry.offset_abs = offset;
		offset += entry.compressed_filesize;

		switch (entry.flags & PHAR_ENT_COMPRESSION_MASK) {
			case PHAR_ENT_COMPRESSED_GZ:
				if (!PHAR_G(has_zlib)) {
					if (entry.metadata) {
						if (entry.is_persistent) {
							free(entry.metadata);
						} else {
							zval_ptr_dtor(&entry.metadata);
						}
					}
					pefree(entry.filename, entry.is_persistent);
					MAPPHAR_FAIL("zlib extension is required for gz compressed .phar file \"%s\"");
				}
				break;
			case PHAR_ENT_COMPRESSED_BZ2:
				if (!PHAR_G(has_bz2)) {
					if (entry.metadata) {
						if (entry.is_persistent) {
							free(entry.metadata);
						} else {
							zval_ptr_dtor(&entry.metadata);
						}
					}
					pefree(entry.filename, entry.is_persistent);
					MAPPHAR_FAIL("bz2 extension is required for bzip2 compressed .phar file \"%s\"");
				}
				break;
			default:
				if (entry.uncompressed_filesize != entry.compressed_filesize) {
					if (entry.metadata) {
						if (entry.is_persistent) {
							free(entry.metadata);
						} else {
							zval_ptr_dtor(&entry.metadata);
						}
					}
					pefree(entry.filename, entry.is_persistent);
					MAPPHAR_FAIL("internal corruption of phar \"%s\" (compressed and uncompressed size does not match for uncompressed entry)");
				}
				break;
		}

		manifest_flags |= (entry.flags & PHAR_ENT_COMPRESSION_MASK);
		/* if signature matched, no need to check CRC32 for each file */
		entry.is_crc_checked = (manifest_flags & PHAR_HDR_SIGNATURE ? 1 : 0);
		phar_set_inode(&entry TSRMLS_CC);
		zend_hash_add(&mydata->manifest, entry.filename, entry.filename_len, (void*)&entry, sizeof(phar_entry_info), NULL);
	}

	snprintf(mydata->version, sizeof(mydata->version), "%u.%u.%u", manifest_ver >> 12, (manifest_ver >> 8) & 0xF, (manifest_ver >> 4) & 0xF);
	mydata->internal_file_start = halt_offset + manifest_len + 4;
	mydata->halt_offset = halt_offset;
	mydata->flags = manifest_flags;
	endbuffer = strrchr(mydata->fname, '/');

	if (endbuffer) {
		mydata->ext = memchr(endbuffer, '.', (mydata->fname + fname_len) - endbuffer);
		if (mydata->ext == endbuffer) {
			mydata->ext = memchr(endbuffer + 1, '.', (mydata->fname + fname_len) - endbuffer - 1);
		}
		if (mydata->ext) {
			mydata->ext_len = (mydata->fname + mydata->fname_len) - mydata->ext;
		}
	}

	mydata->alias = alias ?
		pestrndup(alias, alias_len, mydata->is_persistent) :
		pestrndup(mydata->fname, fname_len, mydata->is_persistent);
	mydata->alias_len = alias ? alias_len : fname_len;
	mydata->sig_flags = sig_flags;
	mydata->fp = fp;
	mydata->sig_len = sig_len;
	mydata->signature = signature;
	phar_request_initialize(TSRMLS_C);

	if (register_alias) {
		phar_archive_data **fd_ptr;

		mydata->is_temporary_alias = temp_alias;

		if (!phar_validate_alias(mydata->alias, mydata->alias_len)) {
			signature = NULL;
			fp = NULL;
			MAPPHAR_FAIL("Cannot open archive \"%s\", invalid alias");
		}

		if (SUCCESS == zend_hash_find(&(PHAR_GLOBALS->phar_alias_map), alias, alias_len, (void **)&fd_ptr)) {
			if (SUCCESS != phar_free_alias(*fd_ptr, alias, alias_len TSRMLS_CC)) {
				signature = NULL;
				fp = NULL;
				MAPPHAR_FAIL("Cannot open archive \"%s\", alias is already in use by existing archive");
			}
		}

		zend_hash_add(&(PHAR_GLOBALS->phar_alias_map), alias, alias_len, (void*)&mydata, sizeof(phar_archive_data*), NULL);
	} else {
		mydata->is_temporary_alias = 1;
	}

	zend_hash_add(&(PHAR_GLOBALS->phar_fname_map), mydata->fname, fname_len, (void*)&mydata, sizeof(phar_archive_data*),  NULL);
	efree(savebuf);

	if (pphar) {
		*pphar = mydata;
	}

	return SUCCESS;
}
