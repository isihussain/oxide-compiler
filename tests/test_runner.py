#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def run():
    root = Path(__file__).resolve().parent.parent
    compiler = root / "bin" / "oxide"

    if not compiler.exists():
        print(f"Error: Compiler binary not found at {compiler}")
        sys.exit(1)

    print("Running Oxide Conformance Test Suite...")
    print("=" * 50)

    # Positive Tests (Execution & Result Check)
    positive_tests = [
        ("examples/01_simple_arithmetic.rs", 42, []),
        ("examples/02_control_flow.rs", 42, []),
        ("examples/03_mutability.rs", 10, []),
        ("examples/04_structs.rs", 42, []),
        ("examples/05_while_loop.rs", 55, []),
        ("examples/06_mutable_struct.rs", 42, []),
        ("examples/07_mutable_reference.rs", 42, []),
        ("examples/08_extern_c_abi.rs", 42, []),
        ("examples/09_aerospace_profile.rs", 42, ["--profile=aerospace"]),
        ("examples/10_tagged_union_enums.rs", 42, []),
        ("examples/11_generics_monomorphization.rs", 42, []),
        ("examples/12_traits_and_impl.rs", 42, []),
        ("examples/13_non_lexical_lifetimes.rs", 42, []),
        ("examples/14_fixed_size_arrays.rs", 42, []),
        ("examples/15_strings_and_macros.rs", 42, []),
        ("examples/16_for_in_loops.rs", 42, []),
        ("examples/17_standard_core_option_result.rs", 42, []),
        ("examples/18_constant_folding.rs", 42, []),
        ("examples/19_dimensional_analysis.rs", 42, []),
        ("examples/20_floating_point_safety.rs", 42, ["--profile=aerospace"]),
        ("examples/21_software_tmr.rs", 42, ["--profile=aerospace"]),
        ("examples/22_mission_state_machine.rs", 42, []),
        ("examples/23_mmio_volatile_barriers.rs", 42, ["--profile=aerospace"]),
        ("examples/24_fixed_point_q16.rs", 42, ["--profile=aerospace"]),
        ("examples/25_realtime_wcet_budget.rs", 42, ["--profile=aerospace"]),
        ("examples/26_actuator_slew_limiter.rs", 42, ["--profile=aerospace"]),
        ("examples/27_afts_corridor_geofence.rs", 42, ["--profile=aerospace"]),
        ("examples/28_spsc_ring_buffer.rs", 42, ["--profile=aerospace"]),
        ("examples/29_mass_depletion_invariants.rs", 42, ["--profile=aerospace"]),
        ("examples/30_bitpacked_telemetry_crc.rs", 42, ["--profile=aerospace"]),
        ("examples/31_cyclomatic_complexity.rs", 42, ["--profile=aerospace"]),
        ("examples/32_double_buffered_table.rs", 42, ["--profile=aerospace"]),
        ("examples/33_adcs_quaternion_norm.rs", 42, ["--profile=aerospace"]),
        ("examples/34_software_bus_pubsub.rs", 42, ["--profile=aerospace"]),
        ("examples/35_watchdog_task_strobe.rs", 42, ["--profile=aerospace"]),
        ("examples/36_sensor_fdir_isolation.rs", 42, ["--profile=aerospace"]),
        ("examples/37_hold_down_interlock.rs", 42, ["--profile=aerospace"]),
        ("examples/38_turbopump_cavitation_monitor.rs", 42, ["--profile=aerospace"]),
        ("examples/39_inertial_disparity_guard.rs", 42, ["--profile=aerospace"]),
        ("examples/40_engine_out_compensation.rs", 42, ["--profile=aerospace"]),
        ("examples/41_control_surface_stall.rs", 42, ["--profile=aerospace"]),
        ("examples/42_touchdown_radar_wow_gate.rs", 42, ["--profile=aerospace"]),
        ("examples/43_algebraic_identities_and_dce.rs", 42, []),
        ("examples/44_loop_unrolling_and_inlining.rs", 42, []),
        ("examples/45_tail_call_optimization.rs", 42, ["--profile=aerospace"]),
        ("examples/46_relational_identities_and_self_simplification.rs", 42, []),
        ("examples/47_bitwise_and_modulo.rs", 42, []),
        ("examples/48_ariane5_bias_overflow.rs", 42, ["--profile=aerospace"]),
        ("examples/49_schiaparelli_descent_gate.rs", 42, ["--profile=aerospace"]),
        ("examples/50_hitomi_momentum_runaway.rs", 42, ["--profile=aerospace"]),
        ("examples/51_pathfinder_priority_ceiling.rs", 42, ["--profile=aerospace"]),
        ("examples/52_vikram_throttle_corridor.rs", 42, ["--profile=aerospace"]),
        ("examples/53_philae_anchoring_interlock.rs", 42, ["--profile=aerospace"]),
        ("examples/54_rms_rate_monotonic_scheduler.rs", 42, ["--profile=aerospace"]),
        ("examples/55_baremetal_atomic_isr_channel.rs", 42, ["--profile=aerospace"]),
        ("examples/56_baremetal_freestanding_guidance.rs", 42, ["--target=baremetal", "--profile=aerospace"]),
        ("examples/57_milstd1553b_flight_bus.rs", 42, ["--profile=aerospace"]),
        ("examples/58_spacewire_packet_routing.rs", 42, ["--profile=aerospace"]),
        ("examples/59_secded_memory_scrubber.rs", 42, ["--profile=aerospace"]),
        ("examples/60_design_by_contract_flight_guidance.rs", 42, ["--profile=aerospace"]),
        ("examples/61_static_stack_bound_analysis.rs", 42, ["--profile=aerospace"]),
        ("examples/62_arinc429_avionics_bus.rs", 42, ["--profile=aerospace"]),
        ("examples/63_can_aerospace_bus.rs", 42, ["--profile=aerospace"]),
    ]

    passed = 0
    total = 0

    for item in positive_tests:
        path_str, expected_exit = item[0], item[1]
        extra_flags = item[2] if len(item) > 2 else []
        total += 1
        src = root / path_str
        bin_out = root / "build" / f"test_{src.stem}"
        cmd = [str(compiler)] + extra_flags + ["-o", str(bin_out), str(src)]
        build_res = subprocess.run(cmd, capture_output=True, text=True)
        if build_res.returncode != 0:
            print(f"[FAIL] {src.name}: compilation failed\n{build_res.stderr}")
            continue

        run_res = subprocess.run([str(bin_out)], capture_output=True)
        if run_res.returncode == expected_exit:
            print(f"[PASS] {src.name} (exit code: {run_res.returncode})")
            passed += 1
        else:
            print(f"[FAIL] {src.name}: expected exit {expected_exit}, got {run_res.returncode}")

    # Negative Tests (Expected compilation failures)
    negative_tests = [
        ("tests/ui/fail_immut_assign.rs", "cannot assign twice to immutable variable", []),
        ("tests/ui/fail_type_mismatch.rs", "mismatched types", []),
        ("tests/ui/fail_uninit.rs", "use of possibly-uninitialized variable", []),
        ("tests/ui/fail_struct_unknown_field.rs", "has no field named", []),
        ("tests/ui/fail_struct_missing_field.rs", "missing field", []),
        ("tests/ui/fail_struct_invalid_field_access.rs", "no field", []),
        ("tests/ui/fail_while_non_bool_condition.rs", "expected 'bool'", []),
        ("tests/ui/fail_assign_immutable_field.rs", "cannot assign to field", []),
        ("tests/ui/fail_deref_immutable_ref_assign.rs", "cannot assign to dereference of immutable reference", []),
        ("tests/ui/fail_extern_type_mismatch.rs", "mismatched type in argument 1", []),
        ("tests/ui/fail_extern_arity_mismatch.rs", "takes 1 arguments but 2 were supplied", []),
        ("tests/ui/fail_aerospace_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_aerospace_mutual_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_aerospace_method_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_match_non_exhaustive.rs", "non-exhaustive patterns: variant 'Blue' not covered", []),
        ("tests/ui/fail_match_arm_type_mismatch.rs", "match arms have incompatible types", []),
        ("tests/ui/fail_enum_payload_mismatch.rs", "mismatched payload type for variant", []),
        ("tests/ui/fail_generic_type_mismatch.rs", "mismatched type in argument 2", []),
        ("tests/ui/fail_generic_struct_field_mismatch.rs", "mismatched type for field 'second'", []),
        ("tests/ui/fail_missing_trait_method.rs", "missing items in 'impl Area for Rectangle': method 'area' not implemented", []),
        ("tests/ui/fail_trait_signature_mismatch.rs", "method 'area' has an incompatible return type", []),
        ("tests/ui/fail_method_not_found.rs", "no method named 'non_existent' found for type 'Point'", []),
        ("tests/ui/fail_immutable_method_call.rs", "cannot borrow immutable local variable 'p' as mutable in method call", []),
        ("tests/ui/fail_nll_mut_borrow_active.rs", "cannot assign to 'x' because it is borrowed", []),
        ("tests/ui/fail_nll_assign_while_borrowed.rs", "cannot assign to 'x' because it is borrowed", []),
        ("tests/ui/fail_nll_double_mut_borrow.rs", "cannot borrow 'x' as mutable more than once at a time", []),
        ("tests/ui/fail_nll_while_loop_borrow.rs", "cannot assign to 'x' because it is borrowed", []),
        ("tests/ui/fail_array_type_mismatch.rs", "mismatched types in array element", []),
        ("tests/ui/fail_array_bounds_check.rs", "index out of bounds", []),
        ("tests/ui/fail_array_assign_immutable.rs", "cannot assign to index of immutable variable", []),
        ("tests/ui/fail_assert_non_bool.rs", "expected 'bool', found 'i32'", []),
        ("tests/ui/fail_string_type_mismatch.rs", "expected 'i32', found '&str'", []),
        ("tests/ui/fail_for_assign_loop_var.rs", "cannot assign twice to immutable variable", []),
        ("tests/ui/fail_range_non_i32.rs", "range bounds must be 'i32'", []),
        ("tests/ui/fail_option_unwrap_type.rs", "expected '&str', found 'i32'", []),
        ("tests/ui/fail_aerospace_unbounded_loop.rs", "unbounded loop detected: NASA/JPL Rule 1", ["--profile=aerospace"]),
        ("tests/ui/fail_aerospace_heap_allocation.rs", "dynamic memory allocation 'malloc' is banned", ["--profile=aerospace"]),
        ("tests/ui/fail_dead_code_type_still_checked.rs", "mismatched types in 'let': expected 'i32', found 'bool'", []),
        ("tests/ui/fail_dimension_add_mismatch.rs", "cannot add differing physical dimensions 'Meter' and 'Second'", []),
        ("tests/ui/fail_dimension_compare_mismatch.rs", "cannot compare differing physical dimensions 'Meter' and 'Newton'", []),
        ("tests/ui/fail_dimension_assign_mismatch.rs", "mismatched types in 'let': expected 'Second', found 'Meter'", []),
        ("tests/ui/fail_float_div_by_zero.rs", "division by zero: floating-point divisor is 0.0", []),
        ("tests/ui/fail_float_sqrt_negative.rs", "domain error: square root of negative float literal", []),
        ("tests/ui/fail_float_clamp_invalid_bounds.rs", "clamp bounds error: min value is greater than max value", []),
        ("tests/ui/fail_float_type_mismatch.rs", "mismatched types in arithmetic: cannot operate on 'f32' and 'i32'", []),
        ("tests/ui/fail_tmr_static_discordance.rs", "radiation mitigation error: static TMR voter discordance", []),
        ("tests/ui/fail_tmr_type_mismatch.rs", "mismatched types across TMR channels", []),
        ("tests/ui/fail_tmr_arity.rs", "tmr_vote takes exactly 3 arguments", []),
        ("tests/ui/fail_state_machine_deadlock.rs", "non-terminal state 'Stuck' has no outgoing transitions", []),
        ("tests/ui/fail_state_machine_deadlock_trap.rs", "deadlock trap detected in state_machine 'TrappedSequence'", []),
        ("tests/ui/fail_state_machine_terminal_transition.rs", "terminal state 'End' cannot have outgoing transitions", []),
        ("tests/ui/fail_state_machine_unknown_state.rs", "unknown state 'NonExistent' in transition", []),
        ("tests/ui/fail_mmio_write_readonly.rs", "cannot write to ReadOnly register", []),
        ("tests/ui/fail_mmio_read_writeonly.rs", "cannot read from WriteOnly register", []),
        ("tests/ui/fail_mmio_missing_barrier.rs", "missing memory barrier between consecutive volatile register writes", ["--profile=aerospace"]),
        ("tests/ui/fail_fixed_point_div_by_zero.rs", "division by zero: fixed-point divisor is 0", []),
        ("tests/ui/fail_fixed_point_type_mismatch.rs", "mismatched types in fixed-point arithmetic", []),
        ("tests/ui/fail_realtime_budget_exceeded.rs", "real-time deadline exceeded in function 'guidance_deadline_exceeded'", ["--profile=aerospace"]),
        ("tests/ui/fail_realtime_budget_invalid.rs", "real-time deadline error in function 'zero_budget_task'", ["--profile=aerospace"]),
        ("tests/ui/fail_actuator_bounds_inverted.rs", "invalid actuator configuration: min_bound (10000) must be strictly less than max_bound (-10000)", []),
        ("tests/ui/fail_actuator_rate_negative.rs", "invalid actuator configuration: max_rate (-500) must be positive", []),
        ("tests/ui/fail_actuator_initial_out_of_bounds.rs", "invalid actuator configuration: initial_pos (15000) exceeds physical bounds [-10000, 10000]", []),
        ("tests/ui/fail_afts_corridor_inverted.rs", "invalid safety corridor: min_x (2000) must be strictly less than max_x (-2000)", []),
        ("tests/ui/fail_afts_velocity_negative.rs", "invalid safety corridor: max_velocity (-500) must be positive", []),
        ("tests/ui/fail_afts_static_breach_pos.rs", "AFTS safety corridor breach detected: position x (3500) exceeds corridor limits [-2000, 2000]", []),
        ("tests/ui/fail_afts_static_breach_vel.rs", "AFTS safety corridor breach detected: velocity magnitude squared (12800) exceeds maximum limit (10000)", []),
        ("tests/ui/fail_spsc_capacity_zero.rs", "invalid SpscRingBuffer capacity (0): must be between 1 and 64", []),
        ("tests/ui/fail_spsc_capacity_exceeded.rs", "invalid SpscRingBuffer capacity (128): must be between 1 and 64", []),
        ("tests/ui/fail_spsc_immutable_push.rs", "cannot borrow immutable local variable 'ring' as mutable in method call", []),
        ("tests/ui/fail_stage_dry_mass_zero.rs", "invalid stage mass configuration: dry_mass (0) must be positive", []),
        ("tests/ui/fail_stage_propellant_negative.rs", "invalid stage mass configuration: initial_propellant (-500) cannot be negative", []),
        ("tests/ui/fail_stage_burn_negative.rs", "cannot burn negative propellant mass (-1000)", []),
        ("tests/ui/fail_stage_dry_mass_breach.rs", "physical dry mass invariant violated: burn amount (60000) exceeds available propellant (50000)", []),
        ("tests/ui/fail_telemetry_packet_id_zero.rs", "invalid telemetry packet: packet_id (0) must be positive", []),
        ("tests/ui/fail_telemetry_timestamp_negative.rs", "invalid telemetry packet: timestamp (-50) cannot be negative", []),
        ("tests/ui/fail_telemetry_channel_out_of_bounds.rs", "telemetry packet channel index (4) out of bounds [0..3]", []),
        ("tests/ui/fail_complexity_threshold_exceeded.rs", "cyclomatic complexity of function 'complex_flight_evaluator' (5) exceeds threshold (3)", ["--profile=aerospace"]),
        ("tests/ui/fail_complexity_attribute_invalid.rs", "cyclomatic complexity configuration error in function 'zero_complexity_task': max_complexity must be a positive integer", ["--profile=aerospace"]),
        ("tests/ui/fail_table_commit_empty_staging.rs", "invalid flight table commit: staging buffer is empty", []),
        ("tests/ui/fail_table_commit_unverified.rs", "unverified flight table commit: staging buffer has not been verified prior to commit", []),
        ("tests/ui/fail_table_bounds_inverted.rs", "invalid flight table bounds: min_val (200) exceeds max_val (100)", []),
        ("tests/ui/fail_table_immutable_load.rs", "cannot borrow immutable local variable 'table' as mutable in method call", []),
        ("tests/ui/fail_quaternion_non_unit_literal.rs", "invalid unit quaternion literal: norm squared (1250000) deviates from unit scale (1000000)", []),
        ("tests/ui/fail_quaternion_arity.rs", "Quaternion::new takes exactly 4 arguments (w, x, y, z)", []),
        ("tests/ui/fail_quaternion_immutable_renormalize.rs", "cannot borrow immutable local variable 'q' as mutable in method call", []),
        ("tests/ui/fail_bus_channel_out_of_bounds.rs", "invalid software bus message ID (9): must be within static channel range [0..7]", []),
        ("tests/ui/fail_bus_channel_negative.rs", "invalid software bus message ID (-1): must be within static channel range [0..7]", []),
        ("tests/ui/fail_bus_immutable_publish.rs", "cannot borrow immutable local variable 'bus' as mutable in method call", []),
        ("tests/ui/fail_watchdog_task_out_of_bounds.rs", "invalid watchdog task ID (5): must be within range [0..3]", []),
        ("tests/ui/fail_watchdog_deadline_zero.rs", "invalid watchdog deadline (0): deadline_ticks must be strictly positive", []),
        ("tests/ui/fail_watchdog_immutable_strobe.rs", "cannot borrow immutable local variable 'wd' as mutable in method call", []),
        ("tests/ui/fail_fdir_bounds_inverted.rs", "invalid sensor FDIR bounds: min_val (500) exceeds max_val (-500)", []),
        ("tests/ui/fail_fdir_threshold_inverted.rs", "invalid sensor FDIR thresholds: isolate_threshold (3) must exceed degrade_threshold (6)", []),
        ("tests/ui/fail_fdir_degrade_zero.rs", "invalid sensor FDIR degrade_threshold (0): must be positive", []),
        ("tests/ui/fail_fdir_immutable_update.rs", "cannot borrow immutable local variable 'fdir' as mutable in method call", []),
        ("tests/ui/fail_hold_down_mode_invalid.rs", "invalid hold down mode (2): expected 0 (StaticFire) or 1 (FlightLaunch)", []),
        ("tests/ui/fail_hold_down_strain_negative.rs", "invalid rated clamp strain (-500): must be positive", []),
        ("tests/ui/fail_hold_down_immutable_update.rs", "cannot borrow immutable local variable 'interlock' as mutable in method call", []),
        ("tests/ui/fail_turbopump_rpm_inverted.rs", "invalid turbopump RPM bounds: max_rpm (10000) must exceed min_rpm (25000)", []),
        ("tests/ui/fail_turbopump_delta_p_negative.rs", "invalid turbopump min_delta_p (-500): must be positive", []),
        ("tests/ui/fail_turbopump_immutable_update.rs", "cannot borrow immutable local variable 'tp' as mutable in method call", []),
        ("tests/ui/fail_inertial_disparity_negative.rs", "invalid inertial max_disparity_cdeg (-100): must be positive", []),
        ("tests/ui/fail_inertial_rate_zero.rs", "invalid inertial max_servo_rate (0): must be positive", []),
        ("tests/ui/fail_inertial_immutable_update.rs", "cannot borrow immutable local variable 'guard' as mutable in method call", []),
        ("tests/ui/fail_compensator_engines_below_two.rs", "invalid cluster total_engines (1): must be at least 2", []),
        ("tests/ui/fail_compensator_delta_v_negative.rs", "invalid cluster target_delta_v (-500): must be positive", []),
        ("tests/ui/fail_compensator_immutable_update.rs", "cannot borrow immutable local variable 'comp' as mutable in method call", []),
        ("tests/ui/fail_surface_count_below_two.rs", "invalid control surface count (1): must be at least 2", []),
        ("tests/ui/fail_surface_error_negative.rs", "invalid max_tracking_error_cdeg (-100): must be positive", []),
        ("tests/ui/fail_surface_immutable_update.rs", "cannot borrow immutable local variable 'guard' as mutable in method call", []),
        ("tests/ui/fail_touchdown_alt_negative.rs", "invalid max_touchdown_alt_cm (-50): must be positive", []),
        ("tests/ui/fail_touchdown_force_zero.rs", "invalid min_wow_force_n (0): must be positive", []),
        ("tests/ui/fail_touchdown_immutable_update.rs", "cannot borrow immutable local variable 'gate' as mutable in method call", []),
        ("tests/ui/fail_tailrec_not_tail_pos.rs", "contains a recursive call that is not in tail position", []),
        ("tests/ui/fail_tailrec_no_recursion.rs", "contains no recursive calls", []),
        ("tests/ui/fail_modulo_by_zero.rs", "division by zero: integer modulo divisor is 0", []),
        ("tests/ui/fail_modulo_type_mismatch.rs", "mismatched types in modulo operation", []),
        ("tests/ui/fail_bitwise_type_mismatch.rs", "mismatched types in bitwise operation", []),
        ("tests/ui/fail_logical_type_mismatch.rs", "mismatched types in logical operation: operands must be 'bool'", []),
        ("tests/ui/fail_bias_max_negative.rs", "invalid max_bias_value (-100): must be positive", []),
        ("tests/ui/fail_bias_max_zero.rs", "invalid max_bias_value (0): must be positive", []),
        ("tests/ui/fail_bias_immutable_update.rs", "cannot borrow immutable local variable 'guard' as mutable in method call", []),
        ("tests/ui/fail_kalman_rate_negative.rs", "invalid max_angular_rate_dps (-100): must be positive", []),
        ("tests/ui/fail_kalman_divergence_zero.rs", "invalid max_divergence_m (0): must be positive", []),
        ("tests/ui/fail_kalman_immutable_update.rs", "cannot borrow immutable local variable 'gate' as mutable in method call", []),
        ("tests/ui/fail_momentum_max_negative.rs", "invalid max_wheel_momentum_nms (-1000): must be positive", []),
        ("tests/ui/fail_momentum_rate_zero.rs", "invalid max_body_rate_cdps (0): must be positive", []),
        ("tests/ui/fail_momentum_immutable_update.rs", "cannot borrow immutable local variable 'guard' as mutable in method call", []),
        ("tests/ui/fail_ceiling_negative.rs", "invalid ceiling_priority (-5): must be positive", []),
        ("tests/ui/fail_ceiling_zero.rs", "invalid ceiling_priority (0): must be positive", []),
        ("tests/ui/fail_ceiling_immutable_lock.rs", "cannot borrow immutable local variable 'mutex' as mutable in method call", []),
        ("tests/ui/fail_throttle_min_negative.rs", "invalid min_throttle_percent (-10): must be positive", []),
        ("tests/ui/fail_throttle_max_excess.rs", "invalid max_throttle_percent (120): cannot exceed 100", []),
        ("tests/ui/fail_throttle_bounds_inverted.rs", "invalid throttle bounds: min_throttle_percent (80) > max_throttle_percent (50)", []),
        ("tests/ui/fail_throttle_slew_zero.rs", "invalid max_slew_rate_percent_per_sec (0): must be positive", []),
        ("tests/ui/fail_throttle_immutable_update.rs", "cannot borrow immutable local variable 'corridor' as mutable in method call", []),
        ("tests/ui/fail_anchoring_strain_negative.rs", "invalid min_gear_strain_n (-50): must be positive", []),
        ("tests/ui/fail_anchoring_penetration_zero.rs", "invalid min_surface_penetration_mm (0): must be positive", []),
        ("tests/ui/fail_anchoring_immutable_update.rs", "cannot borrow immutable local variable 'interlock' as mutable in method call", []),
        ("tests/ui/fail_rms_tasks_zero.rs", "invalid max_tasks (0): must be positive", []),
        ("tests/ui/fail_rms_tasks_excess.rs", "invalid max_tasks (32): cannot exceed 16 in bare-metal static allocation", []),
        ("tests/ui/fail_rms_immutable_add.rs", "cannot borrow immutable local variable 'sched' as mutable in method call", []),
        ("tests/ui/fail_atomic_channel_new_args.rs", "AtomicChannel::new takes 0 arguments", []),
        ("tests/ui/fail_atomic_channel_immutable_send.rs", "cannot borrow immutable local variable 'ch' as mutable in method call", []),
        ("tests/ui/fail_atomic_channel_send_type_mismatch.rs", "AtomicChannel::send parameter must be 'i32'", []),
        ("tests/ui/fail_target_unsupported.rs", "unsupported target 'wasm32'. Supported targets: baremetal, hosted", ["--target=wasm32"]),
        ("tests/ui/fail_milstd1553_invalid_channel.rs", "MilStd1553Bus::new primary_channel must be 0 (Bus A) or 1 (Bus B)", []),
        ("tests/ui/fail_milstd1553_invalid_retries.rs", "MilStd1553Bus::new max_retries must be between 1 and 5", []),
        ("tests/ui/fail_milstd1553_immutable_send.rs", "cannot borrow immutable local variable 'bus' as mutable in method call", []),
        ("tests/ui/fail_milstd1553_rt_out_of_bounds.rs", "MilStd1553Bus::send_command rt_address must be between 0 and 31", []),
        ("tests/ui/fail_milstd1553_word_count_zero.rs", "MilStd1553Bus::send_command word_count must be between 1 and 32", []),
        ("tests/ui/fail_spacewire_invalid_address.rs", "SpaceWireRouter::new node_address must be a logical address between 32 and 254", []),
        ("tests/ui/fail_spacewire_excess_address.rs", "SpaceWireRouter::new node_address must be a logical address between 32 and 254", []),
        ("tests/ui/fail_spacewire_invalid_speed.rs", "SpaceWireRouter::new link_speed_mbps must be between 2 and 200", []),
        ("tests/ui/fail_spacewire_immutable_route.rs", "cannot borrow immutable local variable 'router' as mutable in method call", []),
        ("tests/ui/fail_spacewire_dest_out_of_bounds.rs", "SpaceWireRouter::route_packet dest_addr must be between 32 and 254", []),
        ("tests/ui/fail_spacewire_payload_excess.rs", "SpaceWireRouter::route_packet payload_bytes must be between 1 and 1024", []),
        ("tests/ui/fail_scrubber_invalid_words.rs", "MemoryScrubber::new block_words must be between 1 and 1024", []),
        ("tests/ui/fail_scrubber_excess_words.rs", "MemoryScrubber::new block_words must be between 1 and 1024", []),
        ("tests/ui/fail_scrubber_invalid_interval.rs", "MemoryScrubber::new scrub_interval_ms must be between 1 and 60000", []),
        ("tests/ui/fail_scrubber_interval_excess.rs", "MemoryScrubber::new scrub_interval_ms must be between 1 and 60000", []),
        ("tests/ui/fail_scrubber_immutable_scrub.rs", "cannot borrow immutable local variable 'scrubber' as mutable in method call", []),
        ("tests/ui/fail_contract_static_violation.rs", "violates #[requires] precondition", ["--profile=aerospace"]),
        ("tests/ui/fail_contract_unknown_param.rs", "contract references unknown identifier", ["--profile=aerospace"]),
        ("tests/ui/fail_contract_ensures_unknown_var.rs", "contract references unknown identifier", ["--profile=aerospace"]),
        ("tests/ui/fail_contract_non_bool.rs", "contract condition must evaluate to 'bool'", ["--profile=aerospace"]),
        ("tests/ui/fail_stack_bound_exceeded.rs", "stack bound exceeded in function 'large_frame_task'", ["--profile=aerospace"]),
        ("tests/ui/fail_stack_bound_call_tree_exceeded.rs", "stack bound exceeded in function 'parent_task'", ["--profile=aerospace"]),
        ("tests/ui/fail_stack_bound_invalid.rs", "stack bound configuration error in function 'invalid_stack_task': max_stack_bytes must be a positive integer", ["--profile=aerospace"]),
        ("tests/ui/fail_arinc429_invalid_speed.rs", "Arinc429Channel::new speed_kbps must be 12 (low-speed) or 100 (high-speed)", []),
        ("tests/ui/fail_arinc429_invalid_channel.rs", "Arinc429Channel::new channel_id must be between 0 and 15", []),
        ("tests/ui/fail_arinc429_label_out_of_bounds.rs", "Arinc429Channel::pack_word label must be between 0 and 255 (0o377)", []),
        ("tests/ui/fail_arinc429_immutable_pack.rs", "cannot borrow immutable local variable 'channel' as mutable in method call", []),
        ("tests/ui/fail_can_invalid_baud.rs", "CanAerospaceBus::new baud_rate_kbps must be 125, 250, 500, or 1000", []),
        ("tests/ui/fail_can_load_excess.rs", "CanAerospaceBus::new max_bus_load_pct must be between 1 and 70", []),
        ("tests/ui/fail_can_emergency_priority_inverted.rs", "CanAerospaceBus::transmit_frame emergency channel 0 requires msg_id < 256", []),
        ("tests/ui/fail_can_immutable_transmit.rs", "cannot borrow immutable local variable 'bus' as mutable in method call", []),
    ]

    for item in negative_tests:
        path_str, expected_err_sub = item[0], item[1]
        extra_flags = item[2] if len(item) > 2 else []
        total += 1
        src = root / path_str
        cmd = [str(compiler), "--check"] + extra_flags + [str(src)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0 and expected_err_sub in res.stderr:
            print(f"[PASS] {src.name} (correctly rejected)")
            passed += 1
        else:
            print(f"[FAIL] {src.name}: expected error containing '{expected_err_sub}', output:\n{res.stderr}")

    # Safety Certification Report Verification Tests
    report_tests = [
        ("examples/60_design_by_contract_flight_guidance.rs", "OXIDE DO-178C DAL-A & NASA-STD-8739.8 FLIGHT SAFETY CERTIFICATION REPORT"),
        ("examples/61_static_stack_bound_analysis.rs", "STATIC STACK USAGE BOUND ANALYSIS"),
        ("examples/62_arinc429_avionics_bus.rs", "ARINC 429 Dual-Wire Avionics Bus"),
        ("examples/63_can_aerospace_bus.rs", "ARINC 825 / CAN Aerospace Bus"),
    ]

    for src_rel, expected_text in report_tests:
        total += 1
        src = root / src_rel
        rpt_file = root / "build" / f"test_safety_report_{src.stem}.txt"
        cmd = [str(compiler), f"--emit-safety-report={rpt_file}", str(src)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0 and rpt_file.exists() and expected_text in rpt_file.read_text():
            print(f"[PASS] {src.name} safety report generated")
            passed += 1
        else:
            print(f"[FAIL] {src.name} safety report generation failed:\n{res.stderr}\n{res.stdout}")

    print("=" * 50)
    print(f"Results: {passed}/{total} tests passed.")
    if passed != total:
        sys.exit(1)

if __name__ == "__main__":
    run()
