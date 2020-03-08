#ifndef __I_REPOSITORY_H__
#define __I_REPOSITORY_H__

#include "iBase.h"
#include "err.h"

#define I_REPOSITORY	"iRepository"

#define ST_INITIALIZED	(1<<0)
#define ST_DISABLED	(1<<1)
#define ST_PENDING	(1<<2)

typedef _u8 _cstat_t;


/**
 * Runtime object information.
 */
typedef struct {
	iBase    *pi_base; //!< Pointer to iBase
	_u32  	 ref_cnt; //!< Reference counter
	_cstat_t state; //!< Current state (ST_INITIALIZED, ST_DISABLED or ST_PENDING)
}_base_entry_t;

// object request flags
#define RQ_NAME		(1<<0)
#define RQ_INTERFACE	(1<<1)
#define RQ_VERSION	(1<<2)
// deprecated
#define RQ_CMP_OR	(1<<7) // comparsion type OR (AND is by default)

/**
 * Helper structure.
 *
 * Used to search for objects in arrays of _base_entry_t structures.
 */
typedef struct {
	_u8 		flags;     /*!< @brief Flags for searching.
				    *	This field can be combination of:
				    *	RQ_NAME - search for classname (cname),
				    *	RQ_INTERFACE - search for interface name (iname),
				    *	RQ_VERSION - search for spec. version,
				    */
	_cstr_t		cname;     //!< Const pointer to class name (const char *)
	_cstr_t		iname;     //!< Const pointer to  interface name (const char *)
	_version_t	version;
}_object_request_t;

/**
 * Object handle (pointer to _base_entry_t structure) */
typedef _base_entry_t* HOBJECT;

/**
 * @brief Prototype of enumeration callback.
 * @param[in] file - File name of a extension (shared library)
 * @param[in] alias - Extension alias name.
 * @param[in] p_bentry - Pointer to array of _base_entry_t structures.
 * @param[in] count - Number of _base_entry_t structures in array.
 * @param[in] limit - Limit of the array.
 * @param[in] udata - Pointer to user data.
 */
typedef void _cb_enum_ext_t(_cstr_t file, _cstr_t alias, _base_entry_t *p_bentry, _u32 count, _u32 limit, void *udata);

// notification flags
#define NF_LOAD		(1<<0)
#define NF_INIT		(1<<1)
#define NF_START	(1<<2)
#define NF_STOP		(1<<3)
#define NF_UNINIT	(1<<4)
#define NF_REMOVE	(1<<5)
#define NF_UNLOAD	(1<<6)

/**
 * Interface of a 'repository' component.
 *
 * The repository is a so called spine of a postlink conception.
 * It's used to manage a components and load/unload/enumeration of extensions.
 */
class iRepository: public iBase {
public:
	INTERFACE(iRepository, I_REPOSITORY);
	/**
	 * Retrieve iBase pointer by _object_request_t structure.
	 *
	 * This function can return NULL, if object not found or flags does not matched.
	 * @param[in] request - Pointer to _object_request_t structure, that contains request information.
	 * @param[in] flags - Repository flags (RF_ORIGINAL, RF_CLONE or both).
	 * @return iBase pointer or null.
	 */
	virtual iBase *object_request(_object_request_t *, _rf_t)=0;
	/**
	 * Release iBase pointer.
	 * @param[in] ptr - Pointer to iBase
	 * @param[in] notify - Deprecated
	 */
	virtual void   object_release(iBase *)=0;
	virtual iBase *object_by_cname(_cstr_t cname, _rf_t)=0;
	virtual iBase *object_by_iname(_cstr_t iname, _rf_t)=0;
	virtual iBase *object_by_handle(HOBJECT, _rf_t)=0;
	virtual HOBJECT handle_by_iname(_cstr_t iname)=0;
	virtual HOBJECT handle_by_cname(_cstr_t cname)=0;
	virtual bool object_info(HOBJECT h, _object_info_t *poi)=0;

	virtual void init_array(_base_entry_t *array, _u32 count)=0;

	// extensions
	virtual void extension_dir(_cstr_t dir)=0;
	virtual _cstr_t extension_dir(void)=0;
	virtual _err_t extension_load(_cstr_t file, _cstr_t alias=0)=0;
	virtual _err_t extension_unload(_cstr_t alias)=0;
	virtual void extension_enum(_cb_enum_ext_t *pcb, void *udata)=0;
	virtual void destroy(void)=0;
};

extern iRepository *_gpi_repo_;

#define BY_INAME(iname, flags) \
	_gpi_repo_->object_by_iname(iname, flags)
#define BY_CNAME(cname, flags) \
	_gpi_repo_->object_by_cname(cname, flags)

#endif

