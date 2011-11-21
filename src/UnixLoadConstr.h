// UnixLoadConstr.h
// This is the dynamic loading implementation for UNIX machines
#include "suffix.h"

#include "CycException.h"
#include "Env.h"

#include <dlfcn.h>
#include <iostream>

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
mdl_ctor* Model::loadConstructor(std::string model_type, std::string model_name) {
  mdl_ctor* new_model;

  string start_path = ENV->getCyclusPath();

  if (create_map_.find(model_name) == create_map_.end()) {
    model_name = start_path + "/Models/" + model_type + "/lib" + 
      model_name+SUFFIX;
    void* model = dlopen(model_name.c_str(),RTLD_LAZY);
    if (!model) {
      string err_msg = "Unable to load model shared object file: ";
      err_msg += dlerror();
      throw CycIOException(err_msg);
    }
    new_model = (mdl_ctor*) dlsym(model,"construct");
    if (!new_model) {
      string err_msg = "Unable to load model constructor: ";
      err_msg += dlerror();
      throw CycIOException(err_msg);
    }

    mdl_dtor* del_model = (mdl_dtor*) dlsym(model,"destruct");
    if (!del_model) {
      string err_msg = "Unable to load model destructor: ";
      err_msg += dlerror();
      throw CycIOException(err_msg);
    }

    create_map_[model_name] = new_model;
    destroy_map_[model_name] = del_model;
  } else {
    new_model = create_map_[model_name];
  }
  return new_model;
}
