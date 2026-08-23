/**
 * src/vsim/io/dynx_session.cpp
 * =============================
 * WO-72D / WO-75J  |  Phase 9  |  v5.1.x
 *
 * DynxSession::load() — factory that opens a .dynx archive, reads all rich
 * frames, and returns a fully populated DynxSession ready for playback.
 *
 * Error contract:
 *   On success: session.loaded == true, session.error == ""
 *   On failure: session.loaded == false, session.error contains the reason.
 *               session.frames is empty; no partial state is left behind.
 */

#include "vsim/io/dynx_session.hpp"

namespace vsim {
namespace io {

DynxSession DynxSession::load(const std::string& path) {
	DynxSession session;
	session.path = path;

	DynxReader reader;
	if (!reader.open(path)) {
		session.error = "DynxSession::load: cannot open '" + path + "'";
		return session;
	}

	session.header = reader.header();

	if (!reader.read_all_frames(session.frames)) {
		// read_all_frames returns false if zero frames were read.
		// The file may still be valid but empty — report that clearly.
		session.error = "DynxSession::load: no frames in '" + path + "'";
		if (!reader.error().empty())
			session.error += " (" + reader.error() + ")";
		reader.close();
		return session;
	}
	reader.close();

	// Seed the camera from the first frame's camera state (if any).
	if (!session.frames.empty() && !session.frames[0].cameras.empty())
		session.camera = session.frames[0].cameras[0];

	session.playback.reset();
	session.loaded = true;
	return session;
}

} // namespace io
} // namespace vsim
