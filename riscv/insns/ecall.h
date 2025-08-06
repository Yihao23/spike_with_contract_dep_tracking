switch (STATE.prv)
{
  case PRV_U:
    // std::cerr << "Exception trap_user_ecall\n";
    // std::exit(-1);
    //M:: excp
    // throw trap_user_ecall();
  case PRV_S:
    if (STATE.v){
      // std::cerr << "Exception trap_virtual_supervisor_ecall\n";
      // std::exit(-1);
      //M:: excp
      throw trap_virtual_supervisor_ecall();
    }
    else{
      // std::cerr << "Exception trap_supervisor_ecall\n";
      // std::exit(-1);
      throw trap_supervisor_ecall();
    }
  case PRV_M: 
    // std::cerr << "Exception trap_machine_ecall\n";
    // std::exit(-1);
    throw trap_machine_ecall();
  default: abort();
}
