enum class DeviceState {
    Idle,
    Running,
    Error
};

DeviceState state = DeviceState::Running;

switch (state) {
    case DeviceState::Idle:
        std::cout << "Device is idle.\n";
        break;
    case DeviceState::Running:
        std::cout << "Device is running.\n";
        break;
    case DeviceState::Error:
        std::cout << "Device encountered an error!\n";
        break;
    default:
        std::cout << "Unknown state.\n";
        break;
}
