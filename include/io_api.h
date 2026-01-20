#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// I/O API for XYZ format handling
// Returns 0 on success, non-zero on error

int io_read_xyz(const char* filename, void** molecule_out);
int io_write_xyz(const char* filename, const void* molecule);
int io_free_molecule(void* molecule);

// Error handling
const char* io_get_last_error();

#ifdef __cplusplus
}
#endif
