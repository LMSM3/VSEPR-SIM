#pragma once
/**
 * identity.hpp  -  IDENTITY Library — umbrella header
 *
 * Organises all SM-channel identity-layer content into one includable unit.
 *
 *   IDENTITY scope:
 *     - Matrix2x2 / Matrix3x3 plain-aggregate matrix types
 *     - ParticleIdentity struct (charge, color, Z-matrices, I_recoverable, hash)
 *     - InteractionChannelRow (9 SM-inspired channels)
 *     - ConservationGates, ChannelPolicy, KappaScores, kappa_total()
 *     - InteractionCandidate, evaluate_pair()
 *
 * All headers are header-only with no Qt/GL/OS dependency.
 *
 * Quick include:
 *     #include "IDENTITY/identity.hpp"
 *
 * Fine-grained includes:
 *     #include "IDENTITY/identity_matrix.hpp"
 *     #include "IDENTITY/particle_identity.hpp"
 *     #include "IDENTITY/interaction_channel.hpp"
 *     #include "IDENTITY/interaction_candidate.hpp"
 *
 * Dependency chain (single direction):
 *   identity_matrix
 *       └─ particle_identity
 *               └─ interaction_channel
 *                       └─ interaction_candidate
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include "IDENTITY/identity_matrix.hpp"
#include "IDENTITY/particle_identity.hpp"
#include "IDENTITY/interaction_channel.hpp"
#include "IDENTITY/interaction_candidate.hpp"
