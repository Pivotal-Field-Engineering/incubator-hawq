/*-------------------------------------------------------------------------
 *
 * smgr.h
 *	  storage manager switch public interface declarations.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/smgr.h,v 1.55.2.1 2007/01/27 20:15:47 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef SMGR_H
#define SMGR_H

#include "access/xlog.h"
#include "fmgr.h"
#include "storage/block.h"
#include "storage/relfilenode.h"
#include "storage/dbdirnode.h"
#include "access/persistentendxactrec.h"
#include "access/filerepdefs.h"


/*
 * smgr.c maintains a table of SMgrRelation objects, which are essentially
 * cached file handles.  An SMgrRelation is created (if not already present)
 * by smgropen(), and destroyed by smgrclose().  Note that neither of these
 * operations imply I/O, they just create or destroy a hashtable entry.
 * (But smgrclose() may release associated resources, such as OS-level file
 * descriptors.)
 *
 * An SMgrRelation may have an "owner", which is just a pointer to it from
 * somewhere else; smgr.c will clear this pointer if the SMgrRelation is
 * closed.	We use this to avoid dangling pointers from relcache to smgr
 * without having to make the smgr explicitly aware of relcache.  There
 * can't be more than one "owner" pointer per SMgrRelation, but that's
 * all we need.
 */
typedef struct SMgrRelationData
{
	/* rnode is the hashtable lookup key, so it must be first! */
	RelFileNode smgr_rnode;		/* relation physical identifier */

	/* pointer to owning pointer, or NULL if none */
	struct SMgrRelationData **smgr_owner;

	/* additional public fields may someday exist here */

	/*
	 * Fields below here are intended to be private to smgr.c and its
	 * submodules.	Do not touch them from elsewhere.
	 */
	int			smgr_which;		/* storage manager selector */

	struct _MdMirVec *md_mirvec;		/* for md.c; NULL if not open */
} SMgrRelationData;

typedef SMgrRelationData *SMgrRelation;

/*
 * Whether the object is being dropped in the original transaction, crash recovery, or
 * as part of (re)create / (re)drop during resynchronize.
 */
typedef enum StorageManagerMirrorMode
{
	StorageManagerMirrorMode_None = 0,
	StorageManagerMirrorMode_PrimaryOnly = 1,
	StorageManagerMirrorMode_Both = 2,
	StorageManagerMirrorMode_MirrorOnly = 3,
	MaxStorageManagerMirrorMode /* must always be last */
} StorageManagerMirrorMode;

inline static bool StorageManagerMirrorMode_DoPrimaryWork(
	StorageManagerMirrorMode		mirrorMode)
{
	return (mirrorMode == StorageManagerMirrorMode_Both|| mirrorMode == StorageManagerMirrorMode_PrimaryOnly);
}

inline static bool StorageManagerMirrorMode_SendToMirror(
	StorageManagerMirrorMode		mirrorMode)
{
	return (mirrorMode == StorageManagerMirrorMode_Both|| mirrorMode == StorageManagerMirrorMode_MirrorOnly);
}

extern char *StorageManagerMirrorMode_Name(
	StorageManagerMirrorMode		mirrorMode);

extern void smgrinit(void);
extern SMgrRelation smgropen(RelFileNode rnode);
extern void smgrsetowner(SMgrRelation *owner, SMgrRelation reln);
extern void smgrclose(SMgrRelation reln);
extern void smgrcloseall(void);
extern void smgrclosenode(RelFileNode rnode);
extern void smgrcreatefilespacedirpending(
	Oid 							filespaceOid,

	int4							contentid,

	int16							primaryDbId,

	char							*primaryFilespaceLocation,

	int16							mirrorDbId,

	char							*mirrorFilespaceLocation,

	MirroredObjectExistenceState 	mirrorExistenceState,

	ItemPointer 					persistentTid,

	int64							*persistentSerialNum,

	bool							flushToXLog);
extern void smgrcreatefilespacedir(
	Oid 						filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */

	char						*mirrorFilespaceLocation,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrcreatetablespacedirpending(
	int4							contentid,
	TablespaceDirNode				*tablespaceDirNode,

	MirroredObjectExistenceState	mirrorExistenceState,

	ItemPointer 					persistentTid,

	int64							*persistentSerialNum,
	bool							sharedStorage,

	bool							flushToXLog);
extern void smgrcreatetablespacedir(
	int4						contentid,
	Oid 						tablespaceOid,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrcreatedbdirjustintime(
	int4						contentid,
	DbDirNode					*justInTimeDbDirNode,

	MirroredObjectExistenceState 	mirrorExistenceState,

	StorageManagerMirrorMode	mirrorMode,

	ItemPointer 				persistentTid,

	int64						*persistentSerialNum,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrcreatedbdirpending(
	int4							contentid,
	DbDirNode						*dbDirNode,

	MirroredObjectExistenceState	mirrorExistenceState,

	ItemPointer 					persistentTid,

	int64							*persistentSerialNum,

	bool							flushToXLog);
extern void smgrcreatedbdir(
	int4						contentid,
	DbDirNode					*dbDirNode,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrcreatepending(
	RelFileNode 					*relFileNode,

	int32							segmentFileNum,

	int32							contentid,

	PersistentFileSysRelStorageMgr relStorageMgr,

	PersistentFileSysRelBufpoolKind relBufpoolKind,

	MirroredObjectExistenceState 	mirrorExistenceState,

	MirroredRelDataSynchronizationState relDataSynchronizationState,

	char							*relationName,

	ItemPointer 					persistentTid,

	int64							*persistentSerialNum,

	bool							isLocalBuf,

	bool							bufferPoolBulkLoad,

	bool							flushToXLog);
extern void smgrcreate(
	SMgrRelation 				reln,

	bool						isLocalBuf, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */

	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrrecreate(
	RelFileNode 				*relFileNode,

	bool						isLocalBuf, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */

	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void smgrscheduleunlink(
	RelFileNode 	*relFileNode,

	int32			segmentFileNum,

	int32			contentid,

	PersistentFileSysRelStorageMgr relStorageMgr,

	bool			isLocalBuf,

	char			*relationName,

	ItemPointer 	persistentTid,

	int64			persistentSerialNum);
extern void smgrdounlink(
	RelFileNode 				*relFileNode,

	bool 						isLocalBuf, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	bool  						primaryOnly,

	bool						isRedo,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern void smgrschedulermfilespacedir(
	int4				contentid,
	Oid 				filespaceOid,

	ItemPointer 		persistentTid,

	int64				persistentSerialNum,
	bool				sharedStorage);
extern void smgrschedulermtablespacedir(
	int4				contentid,
	Oid 				tablespaceOid,

	ItemPointer 		persistentTid,

	int64				persistentSerialNum,
	bool				sharedStorage);
extern void smgrschedulermdbdir(
	int4				contentid,
	DbDirNode			*dbDirNode,

	ItemPointer		 	persistentTid,

	int64				persistentSerialNum,
	bool				sharedStorage);
extern void smgrdormfilespacedir(
	Oid 						filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */

	char						*mirrorFilespaceLocation,
	
	bool  						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern void smgrdormtablespacedir(
	int4						contentid,
	Oid 						tablespaceOid,
	
	bool  						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern void smgrdormdbdir(
	int4						contentid,

	DbDirNode 					*dropDbDirNode,
	
	bool  						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern void smgrextend(SMgrRelation reln, BlockNumber blocknum, char *buffer,
		   bool isTemp);
extern void smgrread(SMgrRelation reln, BlockNumber blocknum, char *buffer);
extern void smgrwrite(SMgrRelation reln, BlockNumber blocknum, char *buffer,
		  bool isTemp);
extern BlockNumber smgrnblocks(SMgrRelation reln);
extern BlockNumber smgrtruncate(SMgrRelation reln, BlockNumber nblocks,
			 bool isTemp, bool isLocalBuf, ItemPointer persistentTid, int64 persistentSerialNum);
extern bool smgrgetpersistentinfo(	
	XLogRecord		*record,

	RelFileNode	*relFileNode,

	ItemPointer	persistentTid,

	int64		*persistentSerialNum);
extern void smgrimmedsync(SMgrRelation reln);
/*extern void smgrappendonlymirrorresynceofs(
	RelFileNode						*relFileNode,

	int32							segmentFileNum,

	char							*relationName,

	ItemPointer						persistentTid,

	int64							persistentSerialNum,
	
	bool							mirrorCatchupRequired,

	MirrorDataLossTrackingState 	mirrorDataLossTrackingState,

	int64							mirrorDataLossTrackingSessionNum,

	int64							mirrorNewEof);*/
/*extern bool smgrgetappendonlyinfo(
	RelFileNode 					*relFileNode,

	int32							segmentFileNum,

	char							*relationName,

	bool							*mirrorCatchupRequired,

	MirrorDataLossTrackingState 	*mirrorDataLossTrackingState,

	int64							*mirrorDataLossTrackingSessionNum);*/
extern int	smgrGetPendingFileSysWork(
	EndXactRecKind						endXactRecKind,

	PersistentEndXactFileSysActionInfo	**ptr);
/*extern int	smgrGetAppendOnlyMirrorResyncEofs(
	EndXactRecKind									endXactRecKind,

	PersistentEndXactAppendOnlyMirrorResyncEofs 	**ptr);*/
extern bool	smgrIsPendingFileSysWork(
	EndXactRecKind						endXactRecKind);
/*extern bool smgrIsAppendOnlyMirrorResyncEofs(
	EndXactRecKind						endXactRecKind);*/
extern void AtSubCommit_smgr(void);
extern void AtSubAbort_smgr(void);
extern void AtEOXact_smgr(bool forCommit);
extern void PostPrepare_smgr(void);
extern void smgrcommit(void);
extern void smgrabort(void);
extern void smgrsync(void);

extern void smgr_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record);
extern void smgr_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);


/* internals: move me elsewhere -- ay 7/94 */

/* in md.c */
extern bool mdinit(void);
extern bool mdclose(SMgrRelation reln);
extern void mdcreatefilespacedir(
	Oid 						filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */

	char						*mirrorFilespaceLocation,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void mdcreatetablespacedir(
	int4						contentid,
	Oid 						tablespaceOid,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void mdcreatedbdir(
	int4						contentid,
	DbDirNode					*dbDirNode,

	StorageManagerMirrorMode	mirrorMode,

	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern void mdcreate(
	SMgrRelation 				reln,

	bool 						isLocalBuf, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */

	MirrorDataLossTrackingState mirrorDataLossTrackingState,

	int64						mirrorDataLossTrackingSessionNum,
	
	bool						ignoreAlreadyExists,

	int 						*primaryError,

	bool						*mirrorDataLossOccurred);
extern bool mdunlink(
	RelFileNode 				rnode, 

	char						*relationName,
					/* For tracing only.  Can be NULL in some execution paths. */
	
	bool  						primaryOnly,

	bool						isRedo,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern bool mdrmfilespacedir(
	Oid 						filespaceOid,

	char						*primaryFilespaceLocation,
								/* 
								 * The primary filespace directory path.  NOT Blank padded.
								 * Just a NULL terminated string.
								 */

	char						*mirrorFilespaceLocation,
	
	bool						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern bool mdrmtablespacedir(
	int4						contentid,
	Oid 						tablespaceOid,
	
	bool						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern bool mdrmdbdir(
	int4						contentid,

	DbDirNode					*dbDirNode,
	
	bool						primaryOnly,

	bool						mirrorOnly,

	bool 						ignoreNonExistence,

	bool						*mirrorDataLossOccurred);
extern bool mdextend(SMgrRelation reln, BlockNumber blocknum, char *buffer,
		 bool isTemp);
extern bool mdread(SMgrRelation reln, BlockNumber blocknum, char *buffer);
extern bool mdwrite(SMgrRelation reln, BlockNumber blocknum, char *buffer,
		bool isTemp);
extern BlockNumber mdnblocks(SMgrRelation reln);
extern BlockNumber mdtruncate(SMgrRelation reln, BlockNumber nblocks,
		   bool isTemp);
extern bool mdimmedsync(SMgrRelation reln);
extern bool mdsync(void);

/*
 * MPP-18228 - to make addition to pending delete list atomic with adding
 * a 'Create Pending' entry in persistent tables.
 * Wrapper around the static PendingDelete_AddCreatePendingEntry() for relations
 */
extern void PendingDelete_AddCreatePendingRelationEntry(
		PersistentFileSysObjName *fsObjName,

		ItemPointer 			persistentTid,

		int64					*persistentSerialNum,

		PersistentFileSysRelStorageMgr relStorageMgr,

		char				*relationName,

		bool				isLocalBuf,

		bool				bufferPoolBulkLoad);

/*
 * MPP-18228 - Wrapper around PendingDelete_AddCreatePendingEntry() for
 * database, tablespace and filespace
 */
extern void PendingDelete_AddCreatePendingEntryWrapper(
	PersistentFileSysObjName *fsObjName,

	ItemPointer 			persistentTid,

	int64					persistentSerialNum);

extern void RememberFsyncRequest(RelFileNode rnode, BlockNumber segno);
extern void ForgetRelationFsyncRequests(RelFileNode rnode);
extern void ForgetDatabaseFsyncRequests(Oid tblspc, Oid dbid);

/* smgrtype.c */
extern Datum smgrout(PG_FUNCTION_ARGS);
extern Datum smgrin(PG_FUNCTION_ARGS);
extern Datum smgreq(PG_FUNCTION_ARGS);
extern Datum smgrne(PG_FUNCTION_ARGS);

#endif   /* SMGR_H */
