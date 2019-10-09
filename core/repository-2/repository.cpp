#include <string.h>
#include "private.h"
#include "iTaskMaker.h"

class cRepository: public iRepository {
private:
	_cstr_t	m_ext_dir;
	_set_pi_object_t	ms_pending; // set for  pending objects
	_set_pi_object_t::iterator ms_it_pending; // iterator for pending objects
	iTaskMaker *mpi_tasks;

	void enum_pending(_enum_cb_t *pcb, void *udata) {
		ms_it_pending = ms_pending.begin();

		while(ms_it_pending != ms_pending.end()) {
			iBase *pi_base = *ms_it_pending;
			_s32 x = pcb(pi_base, udata);

			if(x == ENUM_BREAK)
				break;
			else if(x == ENUM_DELETE)
				ms_pending.erase(ms_it_pending);

			ms_it_pending++;
		}
	}

	iTaskMaker *get_task_maker(void) {
		if(!mpi_tasks)
			mpi_tasks = (iTaskMaker *)object_by_iname(I_TASK_MAKER, RF_ORIGINAL);

		return mpi_tasks;
	}

	bool is_original(iBase *pi_base) {
		bool r = false;
		_base_entry_t *p_bentry = find_object_by_pointer(pi_base);

		if(p_bentry)
			// original object context
			r = true;

		return r;
	}

	_cstat_t get_context_state(iBase *pi_base) {
		_cstat_t r = 0;
		_base_entry_t *p_bentry = find_object_by_pointer(pi_base);

		if(p_bentry)
			// original object context
			r = p_bentry->state;
		else
			// dynamic object context
			r = dcs_get_context_state(pi_base);

		return r;
	}

	void set_context_state(iBase *pi_base, _cstat_t state) {
		_base_entry_t *p_bentry = find_object_by_pointer(pi_base);

		if(p_bentry)
			// original object context
			p_bentry->state = state;
		else
			// dynamic object context
			dcs_set_context_state(pi_base, state);
	}

	_base_entry_t *find_object_entry(iBase *pi_base) {
		_base_entry_t *r = find_object_by_pointer(pi_base);

		if(!r) { // cloning may be
			_object_info_t oi;

			pi_base->object_info(&oi);
			r = find_object_by_cname(oi.cname);
		}

		return r;
	}

	void update_users(iBase *pi_base) {
		_u32 count;
		const _link_info_t *pl = pi_base->object_link(&count);

		if(pl) {
			for(_u32 i = 0; i < count; i++) {
				if(*pl[i].ppi_base) {
					_base_entry_t *p_bentry = find_object_entry(*pl[i].ppi_base);
					if(p_bentry)
						users_add_object_user(p_bentry, pi_base);
				}
			}
		}
	}

	// Process original pending list.
	void process_pending_list(_base_entry_t *p_bentry) {
		typedef struct {
			cRepository	*p_repo;
			_base_entry_t	*p_bentry;
		}_enum_info_t;
		_enum_info_t e = {this, p_bentry};

		enum_pending([](iBase *pi_base, void *udata)->_s32 {
			_s32 r = ENUM_CONTINUE;
			_enum_info_t *pe = (_enum_info_t *)udata;

			_u32 lmr = lm_post_init(pi_base, pe->p_bentry, [](_base_entry_t *p_bentry, _rf_t flags, void *udata)->iBase* {
				cRepository *p_repo = (cRepository *)udata;

				return p_repo->object_by_handle(p_bentry, flags);
			}, pe->p_repo);

			pe->p_repo->update_users(pi_base);

			if(lmr & PLMR_READY) {
				if(!(lmr & PLMR_KEEP_PENDING)) {
					r = ENUM_DELETE;
					pe->p_repo->set_context_state(pi_base,
						(pe->p_repo->get_context_state(pi_base) & ~ST_PENDING));
				}
			}

			return r;
		}, &e);
	}

	bool init_object(iBase *pi_base) {
		bool r = false;
		_cstat_t state = get_context_state(pi_base);

		if(!(r = (state & ST_INITIALIZED))) {
			lm_clean(pi_base);

			_u32 lmr = lm_init(pi_base, [](const _link_info_t *pl, void *udata)->iBase* {
				_object_request_t orq;
				cRepository *p_repo = (cRepository *)udata;

				memset(&orq, 0, sizeof(_object_request_t));

				if(pl->iname) {
					orq.flags |= RQ_INTERFACE;
					orq.iname = pl->iname;
				}
				if(pl->cname) {
					orq.flags |= RQ_NAME;
					orq.cname = pl->cname;
				}

				return p_repo->object_request(&orq, pl->flags);
			}, this);

			if(lmr & PLMR_READY) {
				if((r = pi_base->object_ctl(OCTL_INIT, this))) {
					_object_info_t oi;

					state |= ST_INITIALIZED;
					if(lmr & PLMR_KEEP_PENDING) {
						// insert in pending list
						ms_pending.insert(pi_base);
						state |= ST_PENDING;
					}
					set_context_state(pi_base, state);
					update_users(pi_base);

					pi_base->object_info(&oi);
					if(oi.flags & RF_TASK) {
						iTaskMaker *pi_tasks = get_task_maker();

						if(pi_tasks)
							pi_tasks->start(pi_base);
					}
				}
			}
		}

		return r;
	}

	void init_base_array(_base_entry_t *p_bentry, _u32 count) {
		for(_u32 i = 0; i < count; i++) {
			_object_info_t oi;
			bool process_pending = true;

			p_bentry[i].pi_base->object_info(&oi);

			if((oi.flags & RF_ORIGINAL) && !(p_bentry->state & ST_INITIALIZED))
				process_pending = init_object(p_bentry[i].pi_base);

			if(process_pending)
				process_pending_list(&p_bentry[i]);
		}
	}

	bool uninit_object(iBase *pi_base) {
		bool r = false;
		_cstat_t state = get_context_state(pi_base);

		if(state & ST_INITIALIZED) {
			_object_info_t oi;

			pi_base->object_info(&oi);
			if(oi.flags & RF_TASK) {
				iTaskMaker *pi_tasks = get_task_maker();

				if(pi_tasks)
					pi_tasks->stop(pi_base);
			}

			if((r = pi_base->object_ctl(OCTL_UNINIT, this))) {
				lm_uninit(pi_base, [](iBase *pi_base, void *udata) {
					cRepository *p_repo = (cRepository *)udata;

					p_repo->object_release(pi_base, false);
				}, this);

				ms_pending.erase(pi_base);
				set_context_state(pi_base, get_context_state(pi_base) & ~(ST_PENDING | ST_INITIALIZED));
			}
		} else
			r = true;

		return r;
	}

	void uninit_base_array(_base_entry_t *p_bentry, _u32 count) {
		typedef struct {
			cRepository 	*p_repo;
			_base_entry_t	*p_bentry;
		}_enum_info_t;

		for(_u32 i = 0; i < count; i++) {
			_enum_info_t e = {this, &p_bentry[i]};

			users_enum(&p_bentry[i], [](iBase *pi_base, void *udata)->_s32 {
				_enum_info_t *pe = (_enum_info_t *)udata;
				_u32 lmr = lm_remove(pi_base, pe->p_bentry, [](iBase *pi_base, void *udata) {
					cRepository *p_repo = (cRepository *)udata;

					p_repo->object_release(pi_base, false);
				}, pe->p_repo);

				if(lmr & PLMR_UNINIT)
					pe->p_repo->uninit_object(pi_base);
				else {
					if(!(lmr & PLMR_KEEP_PENDING)) {
						pe->p_repo->ms_pending.erase(pi_base);
						pe->p_repo->set_context_state(pi_base,
							pe->p_repo->get_context_state(pi_base) & ~ST_PENDING);
					}
				}

				return ENUM_CONTINUE;
			}, &e);

			users_remove_object(p_bentry);
		}
	}

public:
	BASE(cRepository, "cRepository", RF_ORIGINAL, 2,0,0);

	bool object_ctl(_u32 cmd, void *arg, ...) {
		switch(cmd) {
			case OCTL_INIT:
				mpi_tasks = NULL;
				zinit();
				break;
			case OCTL_UNINIT:
				destroy();
				break;
		}

		return true;
	}

	iBase *object_request(_object_request_t *req, _rf_t flags) {
		iBase *r = NULL;

		//...

		return r;
	}

	void object_release(iBase *pi_base, bool notify=true) {
		//...
	}

	iBase *object_by_cname(_cstr_t cname, _rf_t flags) {
		iBase *r = NULL;

		//...

		return r;
	}

	iBase *object_by_iname(_cstr_t iname, _rf_t flags) {
		iBase *r = NULL;

		//...

		return r;
	}

	iBase *object_by_handle(HOBJECT h, _rf_t flags) {
		iBase *r = NULL;

		//...

		return r;
	}

	HOBJECT handle_by_iname(_cstr_t iname) {
		HOBJECT r = NULL;

		//...

		return r;
	}

	HOBJECT handle_by_cname(_cstr_t cname) {
		HOBJECT r = NULL;

		//...

		return r;
	}

	bool object_info(HOBJECT h, _object_info_t *poi) {
		bool r = false;

		//...

		return r;
	}

	void init_array(_base_entry_t *array, _u32 count) {
		add_base_array(array, count);
		init_base_array(array, count);
	}

	// notifications
	HNOTIFY monitoring_add(iBase *mon_obj, // monitored object
				_cstr_t mon_iname, // monitored interface
				_cstr_t mon_cname, // monitored object name
				iBase *handler_obj,// notifications receiver
				_u8 scan_flags=0 // scan for already registered objects
				) {
		HNOTIFY r = NULL;

		//...

		return r;
	}

	void monitoring_remove(HNOTIFY) {
		//...
	}

	// extensions
	void extension_dir(_cstr_t dir) {
		m_ext_dir = dir;
	}

	_err_t extension_load(_cstr_t file, _cstr_t alias=0) {
		_err_t r = ERR_UNKNOWN;

		//...

		return r;
	}

	_err_t extension_unload(_cstr_t alias) {
		_err_t r = ERR_UNKNOWN;

		//...

		return r;
	}

	void extension_enum(_enum_ext_t *pcb, void *udata) {
		//...
	}

	void destroy(void) {
		//...
	}
};

static cRepository _g_repository_;