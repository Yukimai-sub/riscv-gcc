#ifndef GCC_CFC_H
#define GCC_CFC_H

#define CFC_ATTR_ENABLE 1
#define CFC_ATTR_DISABLE 0
#define CFC_ATTR_UNDEFINED -1


// registered in passes.def
class pass_cfcss : public rtl_opt_pass {
public:
  pass_cfcss();

  opt_pass *clone() override;

  bool gate(function *fun) override;

  unsigned int execute(function *fun) override;
};

// required for registering
rtl_opt_pass *make_pass_cfcss(gcc::context *ctxt);

typedef unsigned char cfcss_sig_t;

#endif
