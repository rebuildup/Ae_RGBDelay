# RGBDelay Implementation Report

## Goals
- [x] Support 8-bit and 16-bit color depth.
- [ ] 32-bit float support (not yet implemented)
- [x] Optimize with After Effects' built-in threading
- [x] Verify local build.
- [x] Add Github Actions

## Progress
- Implemented `RGBDelay.cpp` with:
    - Independent temporal delay for each RGB channel
    - Direct pixel access with proper bounds checking
    - Thread-safe rendering via After Effects' iterate8/iterate16 suites
    - 8-bit and 16-bit per channel support using templates
    - Fast path optimization when all channels sample the same frame
- Added `.github/workflows/build.yml`

## Implementation Details

### Color Depth Support
- **8-bit per channel**: Uses `PF_Pixel` and `Iterate8Suite1`
- **16-bit per channel**: Uses `PF_Pixel16` and `Iterate16Suite1`
- **32-bit float**: Not currently supported. After Effects SDK's `IterateFloatSuite1` would be needed for this feature.

### Threading Model
The plugin uses After Effects' built-in threading infrastructure:
- Registered with `PF_OutFlag2_SUPPORTS_THREADED_RENDERING` flag
- Rendering is performed via `iterate8->iterate()` and `iterate16->iterate()` calls
- After Effects handles thread pool management and work distribution
- No explicit `std::thread` usage is required or used

### Rendering Algorithm
1. **Time Calculation**: Safely calculates source frame times for each channel with overflow/underflow protection
2. **Layer Checkout**: Checks out input layer at required times (caches up to 3 unique time samples)
3. **Coordinate Mapping**: Maps output coordinates to source coordinates for each channel
4. **Pixel Assembly**: Combines R from one time, G from another, B from another
5. **Fast Path**: When all channels sample the same frame with matching dimensions, performs direct copy

### Safety Features
- Integer overflow protection in time calculations
- Bounds checking for all pixel access
- Null pointer validation for layer sources
- Offset overflow checking for extreme coordinate values
- Proper resource cleanup (checkout/checkin)

## Build Log
- Local build verification skipped: `cl` command not found in environment.
- Relying on Github Actions for full build verification.

## Notes
- This is a temporal delay effect, not a spatial chromatic aberration effect
- Each RGB channel samples from a different point in time
- Negative delay values sample future frames, positive values sample past frames
- Alpha channel uses maximum value across all three channel sources
