#include <string.h>
#include "startup.h"
#include "iMemory.h"
#include "iLog.h"
#include "iArgs.h"

using namespace std;
// global pointer to repository
_LOCAL_ iRepository *_gpi_repo_ = 0;

#ifdef _CORE_
_EXPORT_ iRepository *get_repository(void) {
	return _gpi_repo_;
}
void _EXPORT_ register_object(iBase *pi_base) {
#else
void _LOCAL_ register_object(iBase *pi_base) {
#endif
	_base_entry_t e={pi_base, 0, 0};
	add_base_entry(&e);
}

#ifdef _CORE_
typedef struct {
	_cstr_t iname;
	_base_entry_t  *p_entry;
}_early_init_t;

_err_t _EXPORT_ init(int argc, char *argv[]) {
#else
_err_t _EXPORT_ init(iRepository *pi_repo) {
	_gpi_repo_ = pi_repo;
#endif
	_err_t r = ERR_UNKNOWN;
	_u32 i = 0;
	_u32 count, limit;
	_base_entry_t *array = get_base_array(&count, &limit);
#ifdef _CORE_
	_early_init_t ei[]= {
		{I_REPOSITORY,	0}, //0
		{I_HEAP,	0}, //1
		{I_ARGS,	0}, //2
		{I_LOG,		0}, //3
		{0,		0}
	};

	while(i < count) {
		_object_info_t oinfo;
		if(array[i].pi_base) {
			array[i].pi_base->object_info(&oinfo);
			for(_u32 j = 0; ei[j].iname; j++) {
				if(strcmp(oinfo.iname, ei[j].iname) == 0) {
					ei[j].p_entry = &array[i];
					break;
				}
			}
		}
		i++;
	}

	if(ei[0].p_entry && (_gpi_repo_ = (iRepository*)ei[0].p_entry->pi_base)) {
		// the repo is here !
		iHeap *pi_heap=0;
		if(ei[1].p_entry && (pi_heap = (iHeap*)ei[1].p_entry->pi_base)) {
			// the heap is here !
			if(!pi_heap->object_ctl(OCTL_INIT, _gpi_repo_))
				goto _init_done_;
			ei[1].p_entry->state |= ST_INITIALIZED;
			// init repository object
			if(!_gpi_repo_->object_ctl(OCTL_INIT, 0))
				goto _init_done_;
			ei[0].p_entry->state |= ST_INITIALIZED;
			// init args
			iArgs *pi_args = 0;
			if(ei[2].p_entry && (pi_args = (iArgs*)ei[2].p_entry->pi_base)) {
				// args is here
				if(pi_args->object_ctl(OCTL_INIT, _gpi_repo_)) {
					ei[2].p_entry->state |= ST_INITIALIZED;
					pi_args->init(argc, argv);
				}
			}
			// init log
			iLog *pi_log = 0;
			if(ei[3].p_entry && (pi_log = (iLog*)ei[3].p_entry->pi_base)) {
				// log is here
				if(pi_log->object_ctl(OCTL_INIT, _gpi_repo_))
					ei[3].p_entry->state |= ST_INITIALIZED;
			}
		}
	}
#endif
	if(_gpi_repo_) {
		// init everyone else
		i = 0;
		while(i < count) {
			_base_entry_t *pbe = &array[i];

			if(pbe->pi_base && !(pbe->state & ST_INITIALIZED)) {
				_object_info_t oi;

				pbe->pi_base->object_info(&oi);
				if(oi.flags & RF_ORIGINAL) {
					// init here ORIGINAL flagged objects only
					if(pbe->pi_base->object_ctl(OCTL_INIT, _gpi_repo_)) {
						pbe->state |= ST_INITIALIZED;
						if(oi.flags & RF_TASK) {
							// start task
							//...
						}
					}
				}
			}
			i++;
		}
		r = ERR_NONE;
	}
#ifdef _CORE_
_init_done_:
#endif
	return r;
}
