require_extension(EXT_ZCA);
if (!STATE.debug_mode && (
        (!STATE.v && STATE.prv == PRV_M && STATE.dcsr->ebreakm) ||
        (!STATE.v && STATE.prv == PRV_S && STATE.dcsr->ebreaks) ||
        (!STATE.v && STATE.prv == PRV_U && STATE.dcsr->ebreaku) ||
        (STATE.v && STATE.prv == PRV_S && STATE.dcsr->ebreakvs) ||
        (STATE.v && STATE.prv == PRV_U && STATE.dcsr->ebreakvu))) {
	throw trap_debug_mode();
} else {
        // std::cerr << "Exception breakpoint\n";
        // exit(-1);
	throw trap_breakpoint(STATE.v, pc);
}
