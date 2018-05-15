#include "iCmd.h"
#include "iStr.h"
#include "iRepository.h"
#include "iMemory.h"
#include "startup.h"

IMPLEMENT_BASE_ARRAY("libcmd", 10)

typedef struct {
	_str_t	cmd_name;
	iCmd	*pi_cmd;
}_cmd_rec_t;

class cCmdHost: public iCmdHost {
private:
	iLlist	*mpi_cmd_list;
	iStr	*mpi_str;
	HNOTIFY	mh_notify;

	_cmd_t *find_command(iBase *pi_cmd, _str_t cmd_name) {
		_cmd_t *r = 0;
		iCmd *pi_cmd_obj = dynamic_cast<iCmd *>(pi_cmd);

		if(pi_cmd_obj && mpi_cmd_list) {
			_u32 sz = 0;
			HMUTEX hm = mpi_cmd_list->lock();
			_cmd_rec_t *rec = (_cmd_rec_t *)mpi_cmd_list->first(&sz, hm);

			while(rec) {
				if(pi_cmd) {
					if(pi_cmd_obj == rec->pi_cmd) {
						r = rec->pi_cmd->get_info();
						break;
					}
				}
				if(cmd_name) {
					if(mpi_str->str_cmp(cmd_name, rec->cmd_name) == 0 && rec->pi_cmd) {
						r = rec->pi_cmd->get_info();
						break;
					}
				}
				rec = (_cmd_rec_t *)mpi_cmd_list->next(&sz, hm);
			}

			mpi_cmd_list->unlock(hm);
		}

		return r;
	}

	void add_command(iBase *pi_cmd) {
		//...
	}

	void remove_command(iBase *pi_cmd) {
		//...
	}
public:
	BASE(cCmdHost, "cCmdHost", RF_ORIGINAL, 1,0,0);

	bool object_ctl(_u32 cmd, void *arg, ...) {
		bool r = false;

		switch(cmd) {
			case OCTL_INIT: {
					iRepository *pi_repo = (iRepository *)arg;
					mpi_str = (iStr *)pi_repo->object_by_iname(I_STR, RF_ORIGINAL);
					if((mpi_cmd_list = (iLlist *)pi_repo->object_by_iname(I_LLIST, RF_CLONE)))
						mpi_cmd_list->init(LL_VECTOR, 1);
					if(mpi_str && mpi_cmd_list) {
						mh_notify = pi_repo->monitoring_add(0, I_CMD, 0, this, SCAN_ORIGINAL|SCAN_CLONE);
						r = true;
					}
				} break;
			case OCTL_UNINIT: {
					iRepository *pi_repo = (iRepository *)arg;
					if(mh_notify)
						pi_repo->monitoring_remove(mh_notify);
					if(mpi_str)
						pi_repo->object_release(mpi_str);
					if(mpi_cmd_list)
						pi_repo->object_release(mpi_cmd_list);
					r = true;
				} break;
			case OCTL_NOTIFY: {
					_notification_t *pn = (_notification_t *)arg;
					if(pn->flags & NF_INIT)
						add_command(pn->object);
					if(pn->flags & (NF_REMOVE|NF_UNINIT))
						remove_command(pn->object);
					r = true;
				} break;
		}

		return r;
	}

	_cmd_t *get_info(_str_t cmd_name) {
		_cmd_t *r = 0;
		//...
		return r;
	}

	void exec(_str_t cmd_line, iIO *pi_io) {
		//...
	}
};

static cCmdHost _g_cmd_host_;
