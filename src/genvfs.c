/*
 * Copywrite (c) 2022 ThoughtPlot, LLC.
 */
#include "../sqlite/sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <stdlib.h>
#include <string.h>

#include "genvfs.h"
#include "store.h"
#include "common.h"

int genvfs_Close(sqlite3_file *fp) {
	DBG("");
	gen *g = (gen*)fp;
	int err = store_close(g->st);
	if (err) {
		return SQLITE_ERROR;
	}
	return SQLITE_OK;
}

int genvfs_Write(sqlite3_file *fp, const void *buf, int iAmt, sqlite3_int64 iOfst) {
	DBG("%lld %d", iOfst, iAmt);
	gen *g = (gen*)fp;
	int err = store_write(g->st, buf, iOfst, iAmt);
	if (err) {
		DBG("failed");
		return SQLITE_ERROR;
	}
	return SQLITE_OK;
}

int genvfs_Read(sqlite3_file *fp, void *buf, int iAmt, sqlite3_int64 iOfst) {
	DBG("%lld %d", iOfst, iAmt);
	gen *g = (gen*)fp;
	int err = store_read(g->st, buf, iOfst, iAmt);
	if (err == STORE_SHORT_READ){
		int empty = 0;
		for (int i = 0; i < iAmt; i++){
			if (((char*)buf)[i] != 0 ) {
				empty = 1;
			}
		}
		DBG("short read %d",empty);
		return SQLITE_IOERR_SHORT_READ;
	} else if (err) {
		return SQLITE_ERROR;
	}
	return SQLITE_OK;
}

int genvfs_FileSize(sqlite3_file *fp, sqlite3_int64 *pSize) {
	gen *g = (gen*)fp;
	*pSize = store_length(g->st);
	DBG("%lld",*pSize);
	return SQLITE_OK;
}

int genvfs_Truncate(sqlite3_file *fp, sqlite3_int64 size) {
	DBG("");
	gen *g = (gen*)fp;
	store_truncate(g->st, size);
	return SQLITE_OK;
}

int genvfs_Sync(sqlite3_file *fp, int flags) {
	DBG("");
	// all writes are atomic, so this is a noop.
	return SQLITE_OK;
}

int genvfs_Lock(sqlite3_file *fp, int eLock) {
	gen *g = (gen*)fp;
	if (eLock == SQLITE_LOCK_EXCLUSIVE) {
	DBG("exclusive");
		int err = store_lock(g->st);
		if (err) {
			return SQLITE_ERROR;
		}
	} else if (eLock == SQLITE_LOCK_SHARED) {
	DBG("shared");
		int err = store_lock_generation(g->st);
		if (err) {
			return SQLITE_ERROR;
		}
	}
	return SQLITE_OK;
}

int genvfs_Unlock(sqlite3_file *fp, int eLock) {
	gen *g = (gen*)fp;
	if (eLock == SQLITE_LOCK_NONE) {
		DBG("none");
		int err = store_unlock_generation(g->st);
		if (err) {
			return SQLITE_ERROR;
		}
	} else if (eLock == SQLITE_LOCK_SHARED){
		DBG("shared");
		int err = store_unlock(g->st);
		if (err) {
			return SQLITE_ERROR;
		}
	}
	return SQLITE_OK;
}

int genvfs_CheckReservedLock(sqlite3_file *fp, int *pResOut) {
	DBG("");
	// TODO: check with the store
	return !SQLITE_OK;
}

int genvfs_FileControl(sqlite3_file *fp, int op, void *pArg) {
	// TODO: I need to implement a lot more of these
	gen *g = (gen*)fp;
	if( op==SQLITE_FCNTL_PRAGMA ){
		char **azArg = (char**)pArg;
		DBG("%s %s %s",azArg[0], azArg[1], azArg[2]);
		if (sqlite3_stricmp(azArg[1], "generation") == 0) {
			if (azArg[2]) {
				g->st->override_generation = strtoul(azArg[2], NULL, 10);
			}
			DBG("%d %d",g->st->generation, g->st->override_generation);
			azArg[0] = sqlite3_mprintf("%d",g->st->override_generation? g->st->override_generation : g->st->generation);
			return SQLITE_OK;
		}
		/*
		if( sqlite3_stricmp(azArg[1],"checksum_verification")==0 ){
			char *zArg = azArg[2];
			if( zArg!=0 ){
				if( (zArg[0]>='1' && zArg[0]<='9')
					|| sqlite3_strlike("enable%",zArg,0)==0
					|| sqlite3_stricmp("yes",zArg)==0
					|| sqlite3_stricmp("on",zArg)==0
					){
					p->verifyCksm = p->computeCksm;
				}else{
					p->verifyCksm = 0;
				}
				if( p->pPartner ) p->pPartner->verifyCksm = p->verifyCksm;
			}
			azArg[0] = sqlite3_mprintf("%d",p->verifyCksm);
			return SQLITE_OK;
		}else if( p->computeCksm && azArg[2]!=0
			&& sqlite3_stricmp(azArg[1], "page_size")==0 ){
			return SQLITE_OK;
   }
	 */

	} else if ( op == SQLITE_FCNTL_VFSNAME ) {
		snprintf(*(char**)pArg, 7, "genvfs");
		return SQLITE_OK;
	} else if (op == SQLITE_FCNTL_BEGIN_ATOMIC_WRITE) {
		DBG("atomic begin");
		return SQLITE_OK;
	} else if (op == SQLITE_FCNTL_COMMIT_ATOMIC_WRITE) {
		DBG("atomic commit");
		int err = store_promote_generation(g->st);
		if (err) {
			return SQLITE_ERROR;
		}	 
		return SQLITE_OK;
	} else if (op == SQLITE_FCNTL_ROLLBACK_ATOMIC_WRITE) {
		DBG("atomic rollback");
		return SQLITE_OK;
	}
	DBG("%d", op);
	return SQLITE_NOTFOUND;
}

int genvfs_SectorSize(sqlite3_file *fp) {
	DBG("");
	return 4096;
}

int genvfs_DeviceCharacteristics(sqlite3_file *fp) {
	DBG("");
	return (
		//SQLITE_IOCAP_ATOMIC |
		//SQLITE_IOCAP_SAFE_APPEND |
		//SQLITE_IOCAP_SEQUENTIAL |
		SQLITE_IOCAP_POWERSAFE_OVERWRITE |
		SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN |
		SQLITE_IOCAP_BATCH_ATOMIC
		);		
}

const sqlite3_io_methods genvfs_io_methods = {
	1,
	genvfs_Close,
	genvfs_Read,
	genvfs_Write,
	genvfs_Truncate,
	genvfs_Sync,
	genvfs_FileSize,
	genvfs_Lock,
	genvfs_Unlock,
	genvfs_CheckReservedLock,
	genvfs_FileControl,
	genvfs_SectorSize,
	genvfs_DeviceCharacteristics
};

int genvfs_Open(sqlite3_vfs *vfs, const char *zName, sqlite3_file *f, int flags, int *pOutFlags) {
	DBG("%s %x",zName, flags);
	//TODO: this is where we open up the store
	gen *g = (gen*)f;
	g->base.pMethods = &genvfs_io_methods;
	g->st = store_open(zName);
//	*pOutFlags = 0;
	return SQLITE_OK;
}

int genvfs_Delete(sqlite3_vfs *vfs, const char *zName, int syncDir) {
	DBG("");
	// this will never be called.
	// why would you want to delete a whole history of a file?
	return SQLITE_OK;
}

int genvfs_Access(sqlite3_vfs *vfs, const char *zName, int flags, int *pResOut) {
	if (flags == SQLITE_ACCESS_EXISTS) {
		DBG("EX");
	}
	if (flags == SQLITE_ACCESS_READWRITE) {
		DBG("RW");
	}

	*pResOut = 0;
	return SQLITE_OK;
}

int genvfs_FullPathname(sqlite3_vfs *vfs, const char *zName, int nOut, char *zOut) {
	DBG("%s",zName);
	snprintf(zOut, nOut, "%s", zName);
	return SQLITE_OK;
}

static void f_set_generation(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
	DBG("%d", argc);
	sqlite3_vfs *vfs = (sqlite3_vfs*)sqlite3_user_data(ctx);
	sqlite3_result_text(ctx, "asdf", 5, SQLITE_TRANSIENT);
}

static void f_get_generation(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
	DBG("%d", argc);
	sqlite3_vfs *vfs = (sqlite3_vfs*)sqlite3_user_data(ctx);
	sqlite3_result_int64(ctx, 1234);
}

int gen_register(sqlite3* db, char** pzErrMsg, const struct sqlite3_api_routines* pApi) {
	sqlite3_vfs *vfs = sqlite3_vfs_find("gen");
	if (!vfs) {
		return SQLITE_ERROR;
	}

	int rc = sqlite3_create_function(db, "gen_file_copy", 2, SQLITE_UTF8, vfs, f_set_generation, NULL, NULL);
	if (rc) {
		return rc;
	}
	rc = sqlite3_create_function(db, "_get_generation", 0, SQLITE_UTF8, vfs, f_get_generation, NULL, NULL);
	if (rc) {
		return rc;
	}
	return SQLITE_OK;
}

/*
 * init and register the global extention
 */
int sqlite3_genvfs_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
	DBG("");
	SQLITE_EXTENSION_INIT2(pApi);
  sqlite3_vfs* vfs = sqlite3_vfs_find("gen");
	if (!vfs) {
		sqlite3_vfs *defaultVFS = sqlite3_vfs_find(0);
		if (defaultVFS == 0)
			return SQLITE_NOLFS;
		
		vfs = (sqlite3_vfs*) malloc(sizeof(sqlite3_vfs));
		vfs->iVersion = 2;
		vfs->szOsFile = sizeof(struct gen);
		vfs->mxPathname = 100;
		vfs->zName = "gen";
		vfs->pAppData = 0;
		vfs->xOpen = genvfs_Open;
		vfs->xDelete = genvfs_Delete;
		vfs->xAccess = genvfs_Access;
		vfs->xFullPathname = genvfs_FullPathname;

		// we dont need to implement any of these.
		vfs->xDlOpen = defaultVFS->xDlOpen;
		vfs->xDlError = defaultVFS->xDlError;
		vfs->xDlSym = defaultVFS->xDlSym;
		vfs->xDlClose = defaultVFS->xDlClose;

		vfs->xRandomness = defaultVFS->xRandomness;
		vfs->xSleep = defaultVFS->xSleep;
		vfs->xCurrentTime = defaultVFS->xCurrentTime;
		vfs->xGetLastError = defaultVFS->xGetLastError;
		
		vfs->xCurrentTimeInt64 = defaultVFS->xCurrentTimeInt64;
		vfs->xSetSystemCall = defaultVFS->xSetSystemCall;
		vfs->xNextSystemCall = defaultVFS->xNextSystemCall;

		DBG("%d %d %d %d",vfs->xCurrentTime, vfs->xCurrentTimeInt64,vfs->xSetSystemCall,vfs->xNextSystemCall);

		int rc = sqlite3_vfs_register(vfs, 0);

		if (rc) {
			free(vfs);
			return rc;
		}
	}
	int err = sqlite3_auto_extension((void (*)(void))gen_register);
	if (err) {
		return err;
	}
	err = gen_register(db, pzErrMsg, pApi);
	if (err) {
		return err;
	}
	return SQLITE_OK_LOAD_PERMANENTLY;
}

