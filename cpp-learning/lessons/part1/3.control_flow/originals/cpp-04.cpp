switch (cmd_flag) {
    case 'v': // verbose
        enable_logging = true;
        [[fallthrough]]; // Informs the compiler this is intentional
    case 's': // start
        start_device();
        break;
    default:
        stop_device();
}
