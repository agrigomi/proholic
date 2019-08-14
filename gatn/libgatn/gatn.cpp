#include <string.h>
#include "startup.h"
#include "iGatn.h"
#include "iLog.h"
#include "iHT.h"
#include "private.h"

IMPLEMENT_BASE_ARRAY("libgatn", 10);

class cGatn: public iGatn {
private:
	iMap		*mpi_map;// server map
	iLog		*mpi_log;
	_char_t		m_config_fname[256];
	iJSON		*mpi_json;

	void stop(bool autorestore=false) { // stop servers
		_map_enum_t en = mpi_map->enum_open();

		if(en) {
			_u32 sz = 0;
			HMUTEX hm = mpi_map->lock();
			_server_t *p_srv = (_server_t *)mpi_map->enum_first(en, &sz, hm);

			while(p_srv) {
				server *p = dynamic_cast<server *>(p_srv);

				if(p) {
					p->release_network();
					p->release_fs();
					p->m_autorestore = autorestore;
					p_srv = (_server_t *)mpi_map->enum_next(en, &sz, hm);
				}
			}

			mpi_map->unlock(hm);
			mpi_map->enum_close(en);
		}
	}

	void restore(void) {
		_map_enum_t en = mpi_map->enum_open();

		if(en) {
			_u32 sz = 0;
			HMUTEX hm = mpi_map->lock();
			_server_t *p_srv = (_server_t *)mpi_map->enum_first(en, &sz, hm);

			while(p_srv) {
				server *p = dynamic_cast<server *>(p_srv);

				if(p) {
					p->attach_fs();
					p->attach_network();
					if(p->m_autorestore) {
						if(!p->start())
							mpi_log->fwrite(LMT_ERROR, "Gatn: Unable to restore server '%s'", p->name());
					}
					p_srv = (_server_t *)mpi_map->enum_next(en, &sz, hm);
				}
			}

			mpi_map->unlock(hm);
			mpi_map->enum_close(en);
		}
	}

	void destroy(void) {
		_map_enum_t en = mpi_map->enum_open();

		if(en) {
			_u32 sz = 0;
			HMUTEX hm = mpi_map->lock();
			_server_t *p_srv = (_server_t *)mpi_map->enum_first(en, &sz, hm);

			while(p_srv) {
				server *p = dynamic_cast<server *>(p_srv);

				if(p)
					p->destroy();
				p_srv = (_server_t *)mpi_map->enum_next(en, &sz, hm);
			}

			mpi_map->unlock(hm);
			mpi_map->enum_close(en);
		}
	}

	void configure_servers(HTCONTEXT jcxt) {
		HTVALUE htv_srv_array = mpi_json->select(jcxt, "server", NULL);

		if(htv_srv_array) {
			if(mpi_json->type(htv_srv_array) == JVT_ARRAY) {
				//
			} else
				mpi_log->write(LMT_ERROR, "Gatn: Requres array 'server: []'");
		} else
			mpi_log->write(LMT_ERROR, "Gatn: Failed to configure servers");
	}


public:
	BASE(cGatn, "cGatn", RF_ORIGINAL, 1,0,0);

	bool object_ctl(_u32 cmd, void *arg, ...) {
		bool r = false;

		switch(cmd) {
			case OCTL_INIT: {
				iRepository *pi_repo = (iRepository *)arg;

				if((mpi_map = dynamic_cast<iMap *>(pi_repo->object_by_iname(I_MAP, RF_CLONE|RF_NONOTIFY))))
					mpi_map->init(31);
				mpi_log = dynamic_cast<iLog *>(pi_repo->object_by_iname(I_LOG, RF_ORIGINAL));
				init_mime_type_resolver();
				if(mpi_map && mpi_log) {
					pi_repo->monitoring_add(NULL, I_NET, NULL, this, SCAN_ORIGINAL);
					pi_repo->monitoring_add(NULL, I_FS, NULL, this, SCAN_ORIGINAL);
					pi_repo->monitoring_add(NULL, I_GATN_EXTENSION, NULL, this);
					r = true;
				}
			} break;
			case OCTL_UNINIT: {
				iRepository *pi_repo = (iRepository *)arg;

				destroy();
				pi_repo->object_release(mpi_map, false);
				pi_repo->object_release(mpi_log);
				uninit_mime_type_resolver();
				r = true;
			} break;
			case OCTL_NOTIFY: {
				_notification_t *pn = (_notification_t *)arg;
				_object_info_t oi;

				memset(&oi, 0, sizeof(_object_info_t));
				if(pn->object) {
					pn->object->object_info(&oi);

					if(pn->flags & NF_LOAD) {
						if(strcmp(oi.iname, I_GATN_EXTENSION) == 0) {
							// register extension
							mpi_log->fwrite(LMT_INFO, "Gatn: register class '%s'", oi.cname);
							//...
						}
					}
					if(pn->flags & NF_INIT) { // catch
						if(strcmp(oi.iname, I_NET) == 0) {
							mpi_log->write(LMT_INFO, "Gatn: attach networking");
							restore();
						} else if(strcmp(oi.iname, I_FS) == 0) {
							mpi_log->write(LMT_INFO, "Gatn: attach FS");
							restore();
						}
					} else if(pn->flags & (NF_UNINIT | NF_REMOVE)) { // release
						if(strcmp(oi.iname, I_NET) == 0) {
							mpi_log->write(LMT_INFO, "Gatn: detach networking");
							stop(true);
						} else if(strcmp(oi.iname, I_FS) == 0) {
							mpi_log->write(LMT_INFO, "Gatn: detach FS");
							stop(true);
						}
					}
					if(pn->flags & NF_UNLOAD) {
						if(strcmp(oi.iname, I_GATN_EXTENSION) == 0) {
							// unregister extension
							mpi_log->fwrite(LMT_INFO, "Gatn: unregister class '%s'", oi.cname);
							enum_servers([](_server_t *psrv, void *udata) {
								_object_info_t *poi = (_object_info_t *)udata;
								server *ps = (server *)psrv;

								ps->release_class(poi->cname);
							}, &oi);

						}
					}
				}
			} break;
		}

		return r;
	}

	bool configure(_cstr_t json_fname) {
		bool r = false;

		mpi_json = dynamic_cast<iJSON *>(_gpi_repo_->object_by_iname(I_JSON, RF_ORIGINAL));

		if(mpi_json) {
			HTCONTEXT jcxt = mpi_json->create_context();

			if(jcxt) {
				iFS *pi_fs = dynamic_cast<iFS *>(_gpi_repo_->object_by_iname(I_FS, RF_ORIGINAL));

				if(pi_fs) {
					iFileIO *pi_fio = pi_fs->open(json_fname, O_RDONLY);

					if(pi_fio) {
						_cstr_t ptr = (_cstr_t)pi_fio->map(MPF_READ);

						if(ptr) {
							if(mpi_json->parse(jcxt, ptr, pi_fio->size())) {
								configure_servers(jcxt);
							} else
								mpi_log->fwrite(LMT_ERROR, "Gatn: Failed to parse configuration file '%s'", json_fname);
						} else
							mpi_log->fwrite(LMT_ERROR, "Unable to map content of '%s'", json_fname);

						pi_fs->close(pi_fio);
					} else
						mpi_log->fwrite(LMT_ERROR, "Gatn: Unable to open file '%s'", json_fname);

					_gpi_repo_->object_release(pi_fs);
				} else
					mpi_log->write(LMT_ERROR, "Gatn: Unable to obtaian FS interface");

				mpi_json->destroy_context(jcxt);
			} else
				mpi_log->write(LMT_ERROR, "Gatn: Unable to create context for JSON parser");

			_gpi_repo_->object_release(mpi_json);
		} else
			mpi_log->write(LMT_ERROR, "Gatn: Unable to obtain JSON interface");

		return r;
	}

	bool reload(_cstr_t server_name=NULL) {
		bool r = false;

		//...

		return r;
	}

	_server_t *create_server(_cstr_t name, _u32 port,
				_cstr_t doc_root,
				_cstr_t cache_path,
				_cstr_t no_cache="", // non cacheable area inside documents root (by example: folder1:folder2:...)
				_cstr_t path_disable="", // disabled area inside documents root (by example: /folder1/:/folder2/:...)
				_u32 buffer_size=SERVER_BUFFER_SIZE,
				_u32 max_workers=HTTP_MAX_WORKERS,
				_u32 max_connections=HTTP_MAX_CONNECTIONS,
				_u32 connection_timeout=HTTP_CONNECTION_TIMEOUT,
				SSL_CTX *ssl_context=NULL
				) {
		_server_t *r = NULL;
		_u32 sz = 0;

		if(mpi_map) {
			if(!(r = (_server_t *)mpi_map->get(name, strlen(name), &sz))) {
				server srv;
				server *psrv = (server *)mpi_map->add(name, strlen(name), &srv, sizeof(server));

				if(psrv) {
					if(psrv->init(name, port, doc_root, cache_path, no_cache,
							path_disable, buffer_size, max_workers, max_connections,
							connection_timeout, ssl_context)) {
						psrv->start();
						r = psrv;
					}
				}
			}
		}

		return r;
	}

	_server_t *server_by_name(_cstr_t name) {
		_u32 sz = 0;
		_server_t *r = NULL;

		if(name)
			r = (_server_t *)mpi_map->get(name, strlen(name), &sz);

		return r;
	}

	void remove_server(_server_t *p_srv) {
		server *p = dynamic_cast<server *>(p_srv);

		if(p) {
			p->destroy();
			mpi_map->del(p->m_name, strlen(p->m_name));
		}
	}

	bool stop_server(_server_t *p_srv) {
		bool r = false;
		server *ps = dynamic_cast<server *>(p_srv);

		if(ps) {
			ps->stop();
			r = !ps->is_running();
		}

		return r;
	}

	bool start_server(_server_t *p_srv) {
		bool r = false;
		server *ps = dynamic_cast<server *>(p_srv);

		if(ps)
			r = ps->start();

		return r;
	}

	void enum_servers(void (*cb_enum)(_server_t *, void *), void *udata=NULL) {
		_u32 sz = 0;
		_map_enum_t se = mpi_map->enum_open();

		if(se) {
			_server_t *p_srv = reinterpret_cast<_server_t *>(mpi_map->enum_first(se, &sz));

			while(p_srv) {
				cb_enum(p_srv, udata);
				p_srv = reinterpret_cast<_server_t *>(mpi_map->enum_next(se, &sz));
			}

			mpi_map->enum_close(se);
		}
	}
};

static cGatn _g_gatn_;
