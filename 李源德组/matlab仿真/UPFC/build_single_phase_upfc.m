function modelFile = build_single_phase_upfc()
%BUILD_SINGLE_PHASE_UPFC Rebuild the supplied APF as a switching UPFC model.

root = fileparts(mfilename('fullpath'));
sourceFile = fullfile(root, 'B_2025.slx');
modelName = 'SinglePhase_UPFC_Switching';
modelFile = fullfile(root, [modelName '.slx']);

if ~isfile(sourceFile)
    error('Source APF model not found: %s', sourceFile);
end

if bdIsLoaded('B_2025')
    close_system('B_2025', 0);
end
if bdIsLoaded(modelName)
    close_system(modelName, 0);
end

load_system(sourceFile);
save_system('B_2025', modelFile);
close_system('B_2025', 0);
load_system(modelName);
topLines = find_system(modelName, 'FindAll', 'on', 'SearchDepth', 1, 'Type', 'line');
if ~isempty(topLines)
    delete_line(topLines);
end
topBlocks = find_system(modelName, 'SearchDepth', 1, 'Type', 'Block');
if ~isempty(topBlocks)
    delete_block(topBlocks);
end

init = strjoin({ ...
    'f_grid = 50;', ...
    'w0 = 2*pi*f_grid;', ...
    'Vout_ref_rms = 220;', ...
    'Vout_ref_pk = sqrt(2)*Vout_ref_rms;', ...
    'Vgrid_pk = sqrt(2)*220;', ...
    'Vgrid_sag_pk = sqrt(2)*176;', ...
    'Vdc_ref = 400;', ...
    'Cdc = 4700e-6;', ...
    'Lsh = 4e-3;', ...
    'Rsh = 0.35;', ...
    'Lse = 1.5e-3;', ...
    'Rse = 0.20;', ...
    'Kpvdc = 0.45;', ...
    'Kivdc = 8;', ...
    'Kpq = 0.006;', ...
    'Kiq = 0.35;', ...
    'Kpi_sh = 12;', ...
    'Kii_sh = 1200;', ...
    'Kpv_se = 0.8;', ...
    'Kiv_se = 120;', ...
    'wc_v = 2*pi*40;', ...
    'wc_q = 2*pi*25;', ...
    'Qsh_ref_final = 500;', ...
    'fsw = 10000;', ...
    'Ts_power = 2e-6;' ...
    }, newline);
set_param(modelName, 'InitFcn', init, 'PreLoadFcn', '', 'PostLoadFcn', '', ...
    'SolverType', 'Fixed-step', 'Solver', 'ode3', 'FixedStep', 'Ts_power', ...
    'StartTime', '0', 'StopTime', '0.6', 'ReturnWorkspaceOutputs', 'on', ...
    'SignalLogging', 'off', 'SaveOutput', 'off', 'SaveTime', 'off');
evalin('base', init);

build_power_stage(modelName);
build_controls_and_logging(modelName);
add_model_notes(modelName);

set_param(modelName, 'SimulationCommand', 'update');
save_system(modelName, modelFile);
close_system(modelName, 0);
fprintf('Created %s\n', modelFile);
end

function build_power_stage(mdl)
add_block('spspowerguiLib/powergui', [mdl '/powergui'], ...
    'Position', [25 25 120 65], 'SimulationMode', 'Discrete', ...
    'SampleTime', 'Ts_power');

add_block('simulink/Sources/Sine Wave', [mdl '/Grid sine'], ...
    'Position', [25 105 75 135], 'Amplitude', '1', 'Frequency', 'w0', ...
    'Phase', '0', 'SampleTime', '0');
add_block('simulink/Sources/Constant', [mdl '/Grid nominal peak'], ...
    'Position', [25 155 75 185], 'Value', 'Vgrid_pk');
add_block('simulink/Sources/Step', [mdl '/Grid sag'], ...
    'Position', [25 200 75 230], 'Time', '0.15', 'Before', '0', ...
    'After', 'Vgrid_sag_pk-Vgrid_pk');
add_block('simulink/Sources/Step', [mdl '/Grid recovery'], ...
    'Position', [25 245 75 275], 'Time', '0.30', 'Before', '0', ...
    'After', 'Vgrid_pk-Vgrid_sag_pk');
add_block('simulink/Math Operations/Add', [mdl '/Grid amplitude'], ...
    'Position', [115 160 145 250], 'Inputs', '+++');
add_block('simulink/Math Operations/Product', [mdl '/Grid waveform'], ...
    'Position', [175 115 210 150]);
add_block('spsControlledVoltageSourceLib/Controlled Voltage Source', ...
    [mdl '/Grid controlled source'], 'Position', [245 95 300 170]);
add_block('spsGroundLib/Ground', [mdl '/Grid neutral'], ...
    'Position', [245 205 275 235]);
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Grid impedance'], ...
    'Position', [345 95 405 155], 'BranchType', 'RL', ...
    'Resistance', '0.20', 'Inductance', '2e-3');
add_block('spsCurrentMeasurementLib/Current Measurement', [mdl '/Grid current'], ...
    'Position', [440 105 485 145]);

add_block('spsLinearTransformerLib/Linear Transformer', [mdl '/Series injection transformer'], ...
    'Position', [680 75 790 190], 'UNITS', 'pu', ...
    'NominalPower', '[5000 50]', 'winding1', '[220 0.005 0.01]', ...
    'winding2', '[220 0.005 0.01]', 'ThreeWindings', 'off', ...
    'RmLm', '[500 500]', 'Measurements', 'None');
add_block('spsCurrentMeasurementLib/Current Measurement', [mdl '/Load current'], ...
    'Position', [835 105 880 145]);
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Inductive load'], ...
    'Position', [925 90 985 160], 'BranchType', 'RL', ...
    'Resistance', '25', 'Inductance', '10e-3');
add_block('spsGroundLib/Ground', [mdl '/Load neutral'], ...
    'Position', [940 205 970 235]);

add_block('spsVoltageMeasurementLib/Voltage Measurement', [mdl '/PCC voltage'], ...
    'Position', [525 205 570 245]);
add_block('spsGroundLib/Ground', [mdl '/PCC measurement neutral'], ...
    'Position', [520 275 550 305]);
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/PCC damping capacitor'], ...
    'Position', [590 215 630 290], 'BranchType', 'RC', ...
    'Resistance', '1', 'Capacitance', '20e-6');
add_block('spsGroundLib/Ground', [mdl '/PCC filter neutral'], ...
    'Position', [595 315 625 345]);
add_block('spsVoltageMeasurementLib/Voltage Measurement', [mdl '/Output voltage'], ...
    'Position', [845 205 890 245]);
add_block('spsGroundLib/Ground', [mdl '/Output measurement neutral'], ...
    'Position', [840 275 870 305]);
add_block('spsVoltageMeasurementLib/Voltage Measurement', [mdl '/Injected voltage'], ...
    'Position', [690 225 735 265]);

add_bridge(mdl, 'Shunt IGBT bridge', [470 390 590 500]);
add_block('spsCurrentMeasurementLib/Current Measurement', [mdl '/Shunt current'], ...
    'Position', [390 330 435 370]);
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Shunt coupling reactor'], ...
    'Position', [455 310 515 370], 'BranchType', 'RL', ...
    'Resistance', 'Rsh', 'Inductance', 'Lsh');
add_block('spsGroundLib/Ground', [mdl '/Shunt bridge neutral'], ...
    'Position', [545 555 575 585]);

add_bridge(mdl, 'Series IGBT bridge', [740 390 860 500]);
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Series winding reactor'], ...
    'Position', [650 330 710 390], 'BranchType', 'RL', ...
    'Resistance', 'Rse', 'Inductance', 'Lse');
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Series winding filter capacitor'], ...
    'Position', [805 245 845 320], 'BranchType', 'RC', ...
    'Resistance', '2', 'Capacitance', '20e-6');

add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/Shared DC capacitor'], ...
    'Position', [620 460 660 555], 'BranchType', 'C', ...
    'Capacitance', 'Cdc', 'Setx0', 'on', 'InitialVoltage', 'Vdc_ref');
add_block('spsSeriesRLCBranchLib/Series RLC Branch', [mdl '/DC bleed resistor'], ...
    'Position', [685 460 725 555], 'BranchType', 'R', 'Resistance', '20000');
add_block('spsVoltageMeasurementLib/Voltage Measurement', [mdl '/DC link voltage'], ...
    'Position', [610 595 665 635]);

sig(mdl, 'Grid sine', 1, 'Grid waveform', 1);
sig(mdl, 'Grid nominal peak', 1, 'Grid amplitude', 1);
sig(mdl, 'Grid sag', 1, 'Grid amplitude', 2);
sig(mdl, 'Grid recovery', 1, 'Grid amplitude', 3);
sig(mdl, 'Grid amplitude', 1, 'Grid waveform', 2);
sig(mdl, 'Grid waveform', 1, 'Grid controlled source', 1);

phys(mdl, 'Grid controlled source', 'LConn', 1, 'Grid neutral', 'LConn', 1);
phys(mdl, 'Grid controlled source', 'RConn', 1, 'Grid impedance', 'LConn', 1);
phys(mdl, 'Grid impedance', 'RConn', 1, 'Grid current', 'LConn', 1);
phys(mdl, 'Grid current', 'RConn', 1, 'Series injection transformer', 'LConn', 2);
phys(mdl, 'Series injection transformer', 'LConn', 1, 'Load current', 'LConn', 1);
phys(mdl, 'Load current', 'RConn', 1, 'Inductive load', 'LConn', 1);
phys(mdl, 'Inductive load', 'RConn', 1, 'Load neutral', 'LConn', 1);

phys(mdl, 'PCC voltage', 'LConn', 1, 'Grid current', 'RConn', 1);
phys(mdl, 'PCC voltage', 'LConn', 2, 'PCC measurement neutral', 'LConn', 1);
phys(mdl, 'PCC damping capacitor', 'LConn', 1, 'Grid current', 'RConn', 1);
phys(mdl, 'PCC damping capacitor', 'RConn', 1, 'PCC filter neutral', 'LConn', 1);
phys(mdl, 'Output voltage', 'LConn', 1, 'Series injection transformer', 'LConn', 1);
phys(mdl, 'Output voltage', 'LConn', 2, 'Output measurement neutral', 'LConn', 1);
phys(mdl, 'Injected voltage', 'LConn', 1, 'Series injection transformer', 'LConn', 1);
phys(mdl, 'Injected voltage', 'LConn', 2, 'Series injection transformer', 'LConn', 2);

phys(mdl, 'Shunt current', 'LConn', 1, 'Grid current', 'RConn', 1);
phys(mdl, 'Shunt current', 'RConn', 1, 'Shunt coupling reactor', 'LConn', 1);
phys(mdl, 'Shunt coupling reactor', 'RConn', 1, 'Shunt IGBT bridge', 'LConn', 1);
phys(mdl, 'Shunt IGBT bridge', 'LConn', 2, 'Shunt bridge neutral', 'LConn', 1);

phys(mdl, 'Series IGBT bridge', 'LConn', 1, 'Series winding reactor', 'LConn', 1);
phys(mdl, 'Series winding reactor', 'RConn', 1, 'Series injection transformer', 'RConn', 1);
phys(mdl, 'Series IGBT bridge', 'LConn', 2, 'Series injection transformer', 'RConn', 2);
phys(mdl, 'Series winding filter capacitor', 'LConn', 1, 'Series injection transformer', 'RConn', 1);
phys(mdl, 'Series winding filter capacitor', 'RConn', 1, 'Series injection transformer', 'RConn', 2);

phys(mdl, 'Shunt IGBT bridge', 'RConn', 1, 'Series IGBT bridge', 'RConn', 1);
phys(mdl, 'Shunt IGBT bridge', 'RConn', 2, 'Series IGBT bridge', 'RConn', 2);
phys(mdl, 'Shared DC capacitor', 'LConn', 1, 'Shunt IGBT bridge', 'RConn', 1);
phys(mdl, 'Shared DC capacitor', 'RConn', 1, 'Shunt IGBT bridge', 'RConn', 2);
phys(mdl, 'DC bleed resistor', 'LConn', 1, 'Series IGBT bridge', 'RConn', 1);
phys(mdl, 'DC bleed resistor', 'RConn', 1, 'Series IGBT bridge', 'RConn', 2);
phys(mdl, 'DC link voltage', 'LConn', 1, 'Shunt IGBT bridge', 'RConn', 1);
phys(mdl, 'DC link voltage', 'LConn', 2, 'Shunt IGBT bridge', 'RConn', 2);
end

function add_bridge(mdl, name, pos)
add_block('spsUniversalBridgeLib/Universal Bridge', [mdl '/' name], ...
    'Position', pos, 'Arms', '2', 'Device', 'IGBT / Diodes', ...
    'SnubberResistance', '1e5', 'SnubberCapacitance', 'inf', ...
    'Ron', '1e-3', 'Lon', '0', 'ForwardVoltages', '[0.8 0.8]', ...
    'IGBTparameters', '[1e-6 2e-6]', 'Measurements', 'None');
end

function build_controls_and_logging(mdl)
build_shunt_controller(mdl, [mdl '/Shunt Vdc-Q dq controller']);
set_param([mdl '/Shunt Vdc-Q dq controller'], 'Position', [180 700 355 820]);
build_series_controller(mdl, [mdl '/Series voltage dq controller']);
set_param([mdl '/Series voltage dq controller'], 'Position', [560 700 735 805]);
build_pwm(mdl, [mdl '/Shunt unipolar PWM']);
set_param([mdl '/Shunt unipolar PWM'], 'Position', [390 720 500 780]);
build_pwm(mdl, [mdl '/Series unipolar PWM']);
set_param([mdl '/Series unipolar PWM'], 'Position', [770 720 880 780]);

add_block('spsPLLLib/PLL', [mdl '/Continuous PLL'], ...
    'Position', [25 690 105 750], 'Fmin', '45', 'Par_Init', '[0 50]', ...
    'ParK', '[180 3200 1]', 'TcD', '1e-4', 'MaxRateChangeFreq', '12', ...
    'FilterCutOffFreq', '25', 'Ts', '0', 'AGC', 'on');
add_block('simulink/Sources/Step', [mdl '/Shunt Q reference'], ...
    'Position', [25 790 105 820], 'Time', '0.22', 'Before', '0', ...
    'After', 'Qsh_ref_final');

sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'Continuous PLL', 1));
sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'Shunt Vdc-Q dq controller', 1));
sig_h(mdl, out_h(mdl, 'Shunt current'), in_h(mdl, 'Shunt Vdc-Q dq controller', 2));
sig_h(mdl, out_h(mdl, 'DC link voltage'), in_h(mdl, 'Shunt Vdc-Q dq controller', 3));
sig_h(mdl, out_h(mdl, 'Continuous PLL', 2), in_h(mdl, 'Shunt Vdc-Q dq controller', 4));
sig(mdl, 'Shunt Q reference', 1, 'Shunt Vdc-Q dq controller', 5);
sig(mdl, 'Shunt Vdc-Q dq controller', 1, 'Shunt unipolar PWM', 1);
sig(mdl, 'Shunt unipolar PWM', 1, 'Shunt IGBT bridge', 1);

sig_h(mdl, out_h(mdl, 'Output voltage'), in_h(mdl, 'Series voltage dq controller', 1));
sig_h(mdl, out_h(mdl, 'Continuous PLL', 2), in_h(mdl, 'Series voltage dq controller', 2));
sig_h(mdl, out_h(mdl, 'DC link voltage'), in_h(mdl, 'Series voltage dq controller', 3));
sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'Series voltage dq controller', 4));
sig(mdl, 'Series voltage dq controller', 1, 'Series unipolar PWM', 1);
sig(mdl, 'Series unipolar PWM', 1, 'Series IGBT bridge', 1);

add_block('spsRMSLib/RMS', [mdl '/PCC RMS'], 'Position', [1040 95 1100 135], ...
    'TrueRMS', 'on', 'Freq', '50', 'RMSInit', '220', 'Ts', '0');
add_block('spsRMSLib/RMS', [mdl '/Output RMS'], 'Position', [1040 160 1100 200], ...
    'TrueRMS', 'on', 'Freq', '50', 'RMSInit', '220', 'Ts', '0');
sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'PCC RMS', 1));
sig_h(mdl, out_h(mdl, 'Output voltage'), in_h(mdl, 'Output RMS', 1));

add_block('simulink/Signal Routing/Mux', [mdl '/Voltage RMS mux'], ...
    'Position', [1150 105 1155 210], 'Inputs', '3');
add_block('simulink/Sources/Constant', [mdl '/Output RMS reference'], ...
    'Position', [1040 230 1100 260], 'Value', 'Vout_ref_rms');
sig(mdl, 'PCC RMS', 1, 'Voltage RMS mux', 1);
sig(mdl, 'Output RMS', 1, 'Voltage RMS mux', 2);
sig(mdl, 'Output RMS reference', 1, 'Voltage RMS mux', 3);
add_block('simulink/Sinks/Scope', [mdl '/AC voltage regulation'], ...
    'Position', [1200 135 1240 175]);
sig(mdl, 'Voltage RMS mux', 1, 'AC voltage regulation', 1);

add_block('simulink/Signal Routing/Mux', [mdl '/DC voltage mux'], ...
    'Position', [1040 465 1045 535], 'Inputs', '2');
add_block('simulink/Sources/Constant', [mdl '/DC voltage reference'], ...
    'Position', [920 520 980 550], 'Value', 'Vdc_ref');
sig_h(mdl, out_h(mdl, 'DC link voltage'), in_h(mdl, 'DC voltage mux', 1));
sig(mdl, 'DC voltage reference', 1, 'DC voltage mux', 2);
add_block('simulink/Sinks/Scope', [mdl '/DC link regulation'], ...
    'Position', [1090 480 1130 520]);
sig(mdl, 'DC voltage mux', 1, 'DC link regulation', 1);

add_block('simulink/Signal Routing/Mux', [mdl '/Waveform mux'], ...
    'Position', [1040 285 1045 390], 'Inputs', '3');
sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'Waveform mux', 1));
sig_h(mdl, out_h(mdl, 'Output voltage'), in_h(mdl, 'Waveform mux', 2));
sig_h(mdl, out_h(mdl, 'Injected voltage'), in_h(mdl, 'Waveform mux', 3));
add_block('simulink/Sinks/Scope', [mdl '/AC waveforms'], ...
    'Position', [1090 320 1130 360]);
sig(mdl, 'Waveform mux', 1, 'AC waveforms', 1);

add_to_workspace(mdl, 'Log Vdc', [1165 475 1250 505], 'upfc_vdc');
add_to_workspace(mdl, 'Log Vout RMS', [1165 205 1250 235], 'upfc_vout_rms');
add_to_workspace(mdl, 'Log Vpcc RMS', [1165 250 1250 280], 'upfc_vpcc_rms');
add_to_workspace(mdl, 'Log Qsh', [390 820 475 850], 'upfc_qsh');
add_to_workspace(mdl, 'Log Qref', [25 845 110 875], 'upfc_qref');
add_to_workspace(mdl, 'Log Vout waveform', [1165 335 1250 365], 'upfc_vout');
add_to_workspace(mdl, 'Log Vinj waveform', [1165 380 1250 410], 'upfc_vinj');
add_to_workspace(mdl, 'Log Vpcc waveform', [1165 425 1250 455], 'upfc_vpcc');
add_to_workspace(mdl, 'Log Vout d', [760 825 845 855], 'upfc_vout_d');
add_to_workspace(mdl, 'Log Vinj d command', [760 865 845 895], 'upfc_vinj_d_cmd');
add_to_workspace(mdl, 'Log Id reference', [390 865 475 895], 'upfc_id_ref');
add_to_workspace(mdl, 'Log Iq reference', [390 905 475 935], 'upfc_iq_ref');
sig_h(mdl, out_h(mdl, 'DC link voltage'), in_h(mdl, 'Log Vdc', 1));
sig(mdl, 'Output RMS', 1, 'Log Vout RMS', 1);
sig(mdl, 'PCC RMS', 1, 'Log Vpcc RMS', 1);
sig(mdl, 'Shunt Vdc-Q dq controller', 2, 'Log Qsh', 1);
sig(mdl, 'Shunt Q reference', 1, 'Log Qref', 1);
sig_h(mdl, out_h(mdl, 'Output voltage'), in_h(mdl, 'Log Vout waveform', 1));
sig_h(mdl, out_h(mdl, 'Injected voltage'), in_h(mdl, 'Log Vinj waveform', 1));
sig_h(mdl, out_h(mdl, 'PCC voltage'), in_h(mdl, 'Log Vpcc waveform', 1));
sig(mdl, 'Series voltage dq controller', 2, 'Log Vout d', 1);
sig(mdl, 'Series voltage dq controller', 3, 'Log Vinj d command', 1);
sig(mdl, 'Shunt Vdc-Q dq controller', 3, 'Log Id reference', 1);
sig(mdl, 'Shunt Vdc-Q dq controller', 4, 'Log Iq reference', 1);
end

function build_shunt_controller(mdl, sys)
add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', [0 0 100 100]);
Simulink.SubSystem.deleteContents(sys);
names = {'v_pcc', 'i_shunt', 'vdc', 'theta', 'q_ref'};
for k = 1:numel(names)
    add_block('simulink/Sources/In1', [sys '/' names{k}], ...
        'Position', [25 35+80*(k-1) 55 55+80*(k-1)], 'Port', num2str(k));
end
outs = {'m_shunt', 'q_meas', 'id_ref_out', 'iq_ref_out'};
for k = 1:numel(outs)
    add_block('simulink/Sinks/Out1', [sys '/' outs{k}], ...
        'Position', [1060 55+80*(k-1) 1090 75+80*(k-1)], 'Port', num2str(k));
end

add_block('simulink/Continuous/Transfer Fcn', [sys '/PCC quadrature all-pass'], ...
    'Position', [115 25 205 65], 'Numerator', '[-1 w0]', 'Denominator', '[1 w0]');
add_block('simulink/Continuous/Transfer Fcn', [sys '/Current quadrature all-pass'], ...
    'Position', [115 105 205 145], 'Numerator', '[-1 w0]', 'Denominator', '[1 w0]');
add_block('simulink/Math Operations/Trigonometric Function', [sys '/sin theta'], ...
    'Position', [115 270 165 300], 'Operator', 'sin');
add_block('simulink/Math Operations/Trigonometric Function', [sys '/cos theta'], ...
    'Position', [115 320 165 350], 'Operator', 'cos');
build_dq_pair(sys, 'v', [250 25], 'v_pcc', 'PCC quadrature all-pass', 'sin theta', 'cos theta');
build_dq_pair(sys, 'i', [250 135], 'i_shunt', 'Current quadrature all-pass', 'sin theta', 'cos theta');
sig(sys, 'v_pcc', 1, 'PCC quadrature all-pass', 1);
sig(sys, 'i_shunt', 1, 'Current quadrature all-pass', 1);
sig(sys, 'theta', 1, 'sin theta', 1);
sig(sys, 'theta', 1, 'cos theta', 1);

add_block('simulink/Math Operations/Product', [sys '/vq_id'], 'Position', [455 190 490 220]);
add_block('simulink/Math Operations/Product', [sys '/vd_iq'], 'Position', [455 235 490 265]);
add_block('simulink/Math Operations/Add', [sys '/Q difference'], ...
    'Position', [520 200 550 250], 'Inputs', '+-');
add_block('simulink/Math Operations/Gain', [sys '/Q scale'], ...
    'Position', [575 210 625 240], 'Gain', '0.5');
add_block('simulink/Continuous/Transfer Fcn', [sys '/Q measurement LPF'], ...
    'Position', [650 205 735 245], 'Numerator', 'wc_q', 'Denominator', '[1 wc_q]');
sig(sys, 'v_q', 1, 'vq_id', 1); sig(sys, 'i_d', 1, 'vq_id', 2);
sig(sys, 'v_d', 1, 'vd_iq', 1); sig(sys, 'i_q', 1, 'vd_iq', 2);
sig(sys, 'vq_id', 1, 'Q difference', 1); sig(sys, 'vd_iq', 1, 'Q difference', 2);
sig(sys, 'Q difference', 1, 'Q scale', 1);
sig(sys, 'Q scale', 1, 'Q measurement LPF', 1);
sig(sys, 'Q measurement LPF', 1, 'q_meas', 1);

add_block('simulink/Sources/Constant', [sys '/Vdc reference'], ...
    'Position', [250 385 305 415], 'Value', 'Vdc_ref');
add_block('simulink/Math Operations/Add', [sys '/Vdc error'], ...
    'Position', [350 380 380 425], 'Inputs', '+-');
build_pi(sys, 'Vdc PI', [425 365], 'Kpvdc', 'Kivdc');
add_block('simulink/Discontinuities/Saturation', [sys '/id reference limit'], ...
    'Position', [600 375 655 415], 'UpperLimit', '12', 'LowerLimit', '-12');
sig(sys, 'Vdc reference', 1, 'Vdc error', 1); sig(sys, 'vdc', 1, 'Vdc error', 2);
sig(sys, 'Vdc error', 1, 'Vdc PI', 1); sig(sys, 'Vdc PI', 1, 'id reference limit', 1);
sig(sys, 'id reference limit', 1, 'id_ref_out', 1);

add_block('simulink/Math Operations/Add', [sys '/Q error'], ...
    'Position', [350 455 380 500], 'Inputs', '+-');
build_pi(sys, 'Q PI', [425 445], 'Kpq', 'Kiq');
add_block('simulink/Discontinuities/Saturation', [sys '/iq reference limit'], ...
    'Position', [600 455 655 495], 'UpperLimit', '8', 'LowerLimit', '-8');
sig(sys, 'Q measurement LPF', 1, 'Q error', 1); sig(sys, 'q_ref', 1, 'Q error', 2);
sig(sys, 'Q error', 1, 'Q PI', 1); sig(sys, 'Q PI', 1, 'iq reference limit', 1);
sig(sys, 'iq reference limit', 1, 'iq_ref_out', 1);

add_block('simulink/Math Operations/Add', [sys '/id error'], ...
    'Position', [700 370 730 415], 'Inputs', '+-');
add_block('simulink/Math Operations/Add', [sys '/iq error'], ...
    'Position', [700 450 730 495], 'Inputs', '+-');
build_pi(sys, 'id current PI', [765 360], 'Kpi_sh', 'Kii_sh');
build_pi(sys, 'iq current PI', [765 440], 'Kpi_sh', 'Kii_sh');
sig(sys, 'id reference limit', 1, 'id error', 1); sig(sys, 'i_d', 1, 'id error', 2);
sig(sys, 'iq reference limit', 1, 'iq error', 1); sig(sys, 'i_q', 1, 'iq error', 2);
sig(sys, 'id error', 1, 'id current PI', 1); sig(sys, 'iq error', 1, 'iq current PI', 1);

add_block('simulink/Math Operations/Gain', [sys '/minus R id'], ...
    'Position', [690 535 750 565], 'Gain', '-Rsh');
add_block('simulink/Math Operations/Gain', [sys '/wL iq'], ...
    'Position', [690 580 750 610], 'Gain', 'w0*Lsh');
add_block('simulink/Math Operations/Gain', [sys '/minus ud'], ...
    'Position', [865 370 915 400], 'Gain', '-1');
add_block('simulink/Math Operations/Add', [sys '/vconv d'], ...
    'Position', [800 525 835 615], 'Inputs', '++++');
sig(sys, 'v_d', 1, 'vconv d', 1); sig(sys, 'i_d', 1, 'minus R id', 1);
sig(sys, 'minus R id', 1, 'vconv d', 2); sig(sys, 'i_q', 1, 'wL iq', 1);
sig(sys, 'wL iq', 1, 'vconv d', 3); sig(sys, 'id current PI', 1, 'minus ud', 1);
sig(sys, 'minus ud', 1, 'vconv d', 4);

add_block('simulink/Math Operations/Gain', [sys '/minus R iq'], ...
    'Position', [690 645 750 675], 'Gain', '-Rsh');
add_block('simulink/Math Operations/Gain', [sys '/minus wL id'], ...
    'Position', [690 690 750 720], 'Gain', '-w0*Lsh');
add_block('simulink/Math Operations/Gain', [sys '/minus uq'], ...
    'Position', [865 450 915 480], 'Gain', '-1');
add_block('simulink/Math Operations/Add', [sys '/vconv q'], ...
    'Position', [800 635 835 725], 'Inputs', '++++');
sig(sys, 'v_q', 1, 'vconv q', 1); sig(sys, 'i_q', 1, 'minus R iq', 1);
sig(sys, 'minus R iq', 1, 'vconv q', 2); sig(sys, 'i_d', 1, 'minus wL id', 1);
sig(sys, 'minus wL id', 1, 'vconv q', 3); sig(sys, 'iq current PI', 1, 'minus uq', 1);
sig(sys, 'minus uq', 1, 'vconv q', 4);

add_block('simulink/Math Operations/Product', [sys '/vd sin'], 'Position', [870 535 905 565]);
add_block('simulink/Math Operations/Product', [sys '/vq cos'], 'Position', [870 610 905 640]);
add_block('simulink/Math Operations/Add', [sys '/vconv alpha'], ...
    'Position', [935 555 965 625], 'Inputs', '++');
add_block('simulink/Discontinuities/Saturation', [sys '/Vdc denominator'], ...
    'Position', [870 690 930 725], 'LowerLimit', '100', 'UpperLimit', 'inf');
add_block('simulink/Math Operations/Product', [sys '/modulation divide'], ...
    'Position', [985 575 1015 615], 'Inputs', '*/');
add_block('simulink/Discontinuities/Saturation', [sys '/modulation limit'], ...
    'Position', [1025 570 1055 620], 'UpperLimit', '0.95', 'LowerLimit', '-0.95');
sig(sys, 'vconv d', 1, 'vd sin', 1); sig(sys, 'sin theta', 1, 'vd sin', 2);
sig(sys, 'vconv q', 1, 'vq cos', 1); sig(sys, 'cos theta', 1, 'vq cos', 2);
sig(sys, 'vd sin', 1, 'vconv alpha', 1); sig(sys, 'vq cos', 1, 'vconv alpha', 2);
sig(sys, 'vdc', 1, 'Vdc denominator', 1);
sig(sys, 'vconv alpha', 1, 'modulation divide', 1);
sig(sys, 'Vdc denominator', 1, 'modulation divide', 2);
sig(sys, 'modulation divide', 1, 'modulation limit', 1);
sig(sys, 'modulation limit', 1, 'm_shunt', 1);
end

function build_series_controller(~, sys)
add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', [0 0 100 100]);
Simulink.SubSystem.deleteContents(sys);
names = {'v_out', 'theta', 'vdc', 'v_pcc'};
for k = 1:numel(names)
    add_block('simulink/Sources/In1', [sys '/' names{k}], ...
        'Position', [25 45+85*(k-1) 55 65+85*(k-1)], 'Port', num2str(k));
end
outs = {'m_series', 'vout_d_out', 'vinj_d_out'};
for k = 1:numel(outs)
    add_block('simulink/Sinks/Out1', [sys '/' outs{k}], ...
        'Position', [820 55+75*(k-1) 850 75+75*(k-1)], 'Port', num2str(k));
end
add_block('simulink/Continuous/Transfer Fcn', [sys '/Output quadrature all-pass'], ...
    'Position', [115 35 205 75], 'Numerator', '[-1 w0]', 'Denominator', '[1 w0]');
add_block('simulink/Continuous/Transfer Fcn', [sys '/PCC quadrature all-pass'], ...
    'Position', [115 500 205 540], 'Numerator', '[-1 w0]', 'Denominator', '[1 w0]');
add_block('simulink/Math Operations/Trigonometric Function', [sys '/sin theta'], ...
    'Position', [115 135 165 165], 'Operator', 'sin');
add_block('simulink/Math Operations/Trigonometric Function', [sys '/cos theta'], ...
    'Position', [115 185 165 215], 'Operator', 'cos');
build_dq_pair(sys, 'vout', [245 35], 'v_out', 'Output quadrature all-pass', 'sin theta', 'cos theta');
sig(sys, 'v_out', 1, 'Output quadrature all-pass', 1);
sig(sys, 'theta', 1, 'sin theta', 1); sig(sys, 'theta', 1, 'cos theta', 1);
add_block('simulink/Continuous/Transfer Fcn', [sys '/vout d LPF'], ...
    'Position', [375 55 455 90], 'Numerator', 'wc_v', 'Denominator', '[1 wc_v]');
add_block('simulink/Continuous/Transfer Fcn', [sys '/vout q LPF'], ...
    'Position', [375 135 455 170], 'Numerator', 'wc_v', 'Denominator', '[1 wc_v]');
sig(sys, 'vout_d', 1, 'vout d LPF', 1);
sig(sys, 'vout_q', 1, 'vout q LPF', 1);
sig(sys, 'vout d LPF', 1, 'vout_d_out', 1);
build_dq_pair(sys, 'vpcc', [245 500], 'v_pcc', 'PCC quadrature all-pass', 'sin theta', 'cos theta');
sig(sys, 'v_pcc', 1, 'PCC quadrature all-pass', 1);
add_block('simulink/Continuous/Transfer Fcn', [sys '/vpcc d LPF'], ...
    'Position', [375 520 455 555], 'Numerator', 'wc_v', 'Denominator', '[1 wc_v]');
add_block('simulink/Continuous/Transfer Fcn', [sys '/vpcc q LPF'], ...
    'Position', [375 600 455 635], 'Numerator', 'wc_v', 'Denominator', '[1 wc_v]');
sig(sys, 'vpcc_d', 1, 'vpcc d LPF', 1);
sig(sys, 'vpcc_q', 1, 'vpcc q LPF', 1);

add_block('simulink/Sources/Constant', [sys '/Output d reference'], ...
    'Position', [245 280 305 310], 'Value', 'Vout_ref_pk');
add_block('simulink/Sources/Constant', [sys '/Output q reference'], ...
    'Position', [245 355 305 385], 'Value', '0');
add_block('simulink/Math Operations/Add', [sys '/d voltage error'], ...
    'Position', [350 270 380 315], 'Inputs', '+-');
add_block('simulink/Math Operations/Add', [sys '/q voltage error'], ...
    'Position', [350 345 380 390], 'Inputs', '+-');
build_pi(sys, 'd voltage PI', [420 260], 'Kpv_se', 'Kiv_se');
build_pi(sys, 'q voltage PI', [420 335], 'Kpv_se', 'Kiv_se');
add_block('simulink/Sources/Step', [sys '/Series control enable'], ...
    'Position', [325 430 380 460], 'Time', '0.06', 'Before', '0', 'After', '1');
add_block('simulink/Math Operations/Product', [sys '/Enabled d error'], ...
    'Position', [390 275 420 305]);
add_block('simulink/Math Operations/Product', [sys '/Enabled q error'], ...
    'Position', [390 350 420 380]);
add_block('simulink/Discontinuities/Saturation', [sys '/vinj d limit'], ...
    'Position', [585 270 640 310], 'UpperLimit', '100', 'LowerLimit', '-100');
add_block('simulink/Discontinuities/Saturation', [sys '/vinj q limit'], ...
    'Position', [585 345 640 385], 'UpperLimit', '100', 'LowerLimit', '-100');
add_block('simulink/Math Operations/Add', [sys '/d voltage feedforward'], ...
    'Position', [480 500 510 545], 'Inputs', '+-');
add_block('simulink/Math Operations/Add', [sys '/q voltage feedforward'], ...
    'Position', [480 575 510 620], 'Inputs', '+-');
add_block('simulink/Math Operations/Add', [sys '/d injection command'], ...
    'Position', [545 270 575 315], 'Inputs', '++');
add_block('simulink/Math Operations/Add', [sys '/q injection command'], ...
    'Position', [545 345 575 390], 'Inputs', '++');
sig(sys, 'Output d reference', 1, 'd voltage error', 1); sig(sys, 'vout d LPF', 1, 'd voltage error', 2);
sig(sys, 'Output q reference', 1, 'q voltage error', 1); sig(sys, 'vout q LPF', 1, 'q voltage error', 2);
sig(sys, 'd voltage error', 1, 'Enabled d error', 1);
sig(sys, 'q voltage error', 1, 'Enabled q error', 1);
sig(sys, 'Series control enable', 1, 'Enabled d error', 2);
sig(sys, 'Series control enable', 1, 'Enabled q error', 2);
sig(sys, 'Enabled d error', 1, 'd voltage PI', 1);
sig(sys, 'Enabled q error', 1, 'q voltage PI', 1);
sig(sys, 'Output d reference', 1, 'd voltage feedforward', 1);
sig(sys, 'vpcc d LPF', 1, 'd voltage feedforward', 2);
sig(sys, 'Output q reference', 1, 'q voltage feedforward', 1);
sig(sys, 'vpcc q LPF', 1, 'q voltage feedforward', 2);
sig(sys, 'd voltage PI', 1, 'd injection command', 1);
sig(sys, 'd voltage feedforward', 1, 'd injection command', 2);
sig(sys, 'q voltage PI', 1, 'q injection command', 1);
sig(sys, 'q voltage feedforward', 1, 'q injection command', 2);
sig(sys, 'd injection command', 1, 'vinj d limit', 1);
sig(sys, 'q injection command', 1, 'vinj q limit', 1);
sig(sys, 'vinj d limit', 1, 'vinj_d_out', 1);

add_block('simulink/Math Operations/Product', [sys '/vinjd sin'], 'Position', [665 260 700 290]);
add_block('simulink/Math Operations/Product', [sys '/vinjq cos'], 'Position', [665 335 700 365]);
add_block('simulink/Math Operations/Add', [sys '/vinj alpha'], ...
    'Position', [725 280 755 350], 'Inputs', '++');
add_block('simulink/Discontinuities/Saturation', [sys '/Vdc denominator'], ...
    'Position', [585 430 640 465], 'LowerLimit', '100', 'UpperLimit', 'inf');
add_block('simulink/Math Operations/Product', [sys '/modulation divide'], ...
    'Position', [730 390 760 430], 'Inputs', '*/');
add_block('simulink/Discontinuities/Saturation', [sys '/modulation limit'], ...
    'Position', [780 385 810 435], 'UpperLimit', '0.95', 'LowerLimit', '-0.95');
sig(sys, 'vinj d limit', 1, 'vinjd sin', 1); sig(sys, 'sin theta', 1, 'vinjd sin', 2);
sig(sys, 'vinj q limit', 1, 'vinjq cos', 1); sig(sys, 'cos theta', 1, 'vinjq cos', 2);
sig(sys, 'vinjd sin', 1, 'vinj alpha', 1); sig(sys, 'vinjq cos', 1, 'vinj alpha', 2);
sig(sys, 'vdc', 1, 'Vdc denominator', 1);
sig(sys, 'vinj alpha', 1, 'modulation divide', 1);
sig(sys, 'Vdc denominator', 1, 'modulation divide', 2);
sig(sys, 'modulation divide', 1, 'modulation limit', 1);
sig(sys, 'modulation limit', 1, 'm_series', 1);
end

function build_dq_pair(sys, prefix, xy, alpha, beta, sinb, cosb)
x = xy(1); y = xy(2);
blocks = { ...
    [prefix '_alpha_sin'], [x y x+35 y+30]; ...
    [prefix '_beta_cos'], [x y+40 x+35 y+70]; ...
    [prefix '_d'], [x+65 y+15 x+95 y+55]; ...
    [prefix '_alpha_cos'], [x y+75 x+35 y+105]; ...
    [prefix '_beta_sin'], [x y+115 x+35 y+145]; ...
    [prefix '_q'], [x+65 y+90 x+95 y+130]};
for k = [1 2 4 5]
    add_block('simulink/Math Operations/Product', [sys '/' blocks{k,1}], 'Position', blocks{k,2});
end
add_block('simulink/Math Operations/Add', [sys '/' blocks{3,1}], ...
    'Position', blocks{3,2}, 'Inputs', '+-');
add_block('simulink/Math Operations/Add', [sys '/' blocks{6,1}], ...
    'Position', blocks{6,2}, 'Inputs', '++');
sig(sys, alpha, 1, blocks{1,1}, 1); sig(sys, sinb, 1, blocks{1,1}, 2);
sig(sys, beta, 1, blocks{2,1}, 1); sig(sys, cosb, 1, blocks{2,1}, 2);
sig(sys, blocks{1,1}, 1, blocks{3,1}, 1); sig(sys, blocks{2,1}, 1, blocks{3,1}, 2);
sig(sys, alpha, 1, blocks{4,1}, 1); sig(sys, cosb, 1, blocks{4,1}, 2);
sig(sys, beta, 1, blocks{5,1}, 1); sig(sys, sinb, 1, blocks{5,1}, 2);
sig(sys, blocks{4,1}, 1, blocks{6,1}, 1); sig(sys, blocks{5,1}, 1, blocks{6,1}, 2);
end

function build_pi(sys, name, xy, kp, ki)
x = xy(1); y = xy(2);
add_block('simulink/Ports & Subsystems/Subsystem', [sys '/' name], ...
    'Position', [x y x+130 y+55]);
piSys = [sys '/' name];
Simulink.SubSystem.deleteContents(piSys);
add_block('simulink/Sources/In1', [piSys '/e'], 'Position', [20 30 50 50]);
add_block('simulink/Math Operations/Gain', [piSys '/Kp'], ...
    'Position', [80 10 120 40], 'Gain', kp);
add_block('simulink/Math Operations/Gain', [piSys '/Ki'], ...
    'Position', [80 60 120 90], 'Gain', ki);
add_block('simulink/Continuous/Integrator', [piSys '/Continuous integrator'], ...
    'Position', [150 55 180 95], 'InitialCondition', '0');
add_block('simulink/Math Operations/Add', [piSys '/P plus I'], ...
    'Position', [210 25 240 75], 'Inputs', '++');
add_block('simulink/Sinks/Out1', [piSys '/u'], 'Position', [275 40 305 60]);
sig(piSys, 'e', 1, 'Kp', 1); sig(piSys, 'e', 1, 'Ki', 1);
sig(piSys, 'Ki', 1, 'Continuous integrator', 1);
sig(piSys, 'Kp', 1, 'P plus I', 1); sig(piSys, 'Continuous integrator', 1, 'P plus I', 2);
sig(piSys, 'P plus I', 1, 'u', 1);
end

function build_pwm(~, sys)
add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', [0 0 100 100]);
Simulink.SubSystem.deleteContents(sys);
add_block('simulink/Sources/In1', [sys '/m'], 'Position', [25 85 55 105]);
add_block('simulink/Math Operations/Gain', [sys '/minus m'], ...
    'Position', [100 125 145 155], 'Gain', '-1');
add_block('simulink/Sources/Repeating Sequence', [sys '/Triangular carrier'], ...
    'Position', [95 25 175 55], 'rep_seq_t', '[0 0.5 1]/fsw', ...
    'rep_seq_y', '[-1 1 -1]');
add_block('simulink/Logic and Bit Operations/Relational Operator', [sys '/Leg A compare'], ...
    'Position', [205 70 245 105], 'Operator', '>=');
add_block('simulink/Logic and Bit Operations/Relational Operator', [sys '/Leg B compare'], ...
    'Position', [205 130 245 165], 'Operator', '>=');
add_block('simulink/Logic and Bit Operations/Logical Operator', [sys '/Leg A lower'], ...
    'Position', [285 75 325 105], 'Operator', 'NOT');
add_block('simulink/Logic and Bit Operations/Logical Operator', [sys '/Leg B lower'], ...
    'Position', [285 135 325 165], 'Operator', 'NOT');
add_block('simulink/Signal Routing/Mux', [sys '/Gate vector'], ...
    'Position', [370 55 375 185], 'Inputs', '4');
add_block('simulink/Sinks/Out1', [sys '/g'], 'Position', [420 105 450 125]);
sig(sys, 'm', 1, 'Leg A compare', 1); sig(sys, 'm', 1, 'minus m', 1);
sig(sys, 'minus m', 1, 'Leg B compare', 1);
sig(sys, 'Triangular carrier', 1, 'Leg A compare', 2);
sig(sys, 'Triangular carrier', 1, 'Leg B compare', 2);
sig(sys, 'Leg A compare', 1, 'Leg A lower', 1);
sig(sys, 'Leg B compare', 1, 'Leg B lower', 1);
sig(sys, 'Leg A compare', 1, 'Gate vector', 1);
sig(sys, 'Leg A lower', 1, 'Gate vector', 2);
sig(sys, 'Leg B compare', 1, 'Gate vector', 3);
sig(sys, 'Leg B lower', 1, 'Gate vector', 4);
sig(sys, 'Gate vector', 1, 'g', 1);
end

function add_to_workspace(mdl, name, pos, variable)
add_block('simulink/Sinks/To Workspace', [mdl '/' name], 'Position', pos, ...
    'VariableName', variable, 'SaveFormat', 'Structure With Time', ...
    'MaxDataPoints', 'inf', 'Decimation', '10');
end

function add_model_notes(mdl)
note = Simulink.Annotation(mdl, sprintf(['Single-phase switching UPFC (not an average model)\n' ...
    'Shunt VSC: Vdc + reactive-power outer loops, dq current PI inner loops\n' ...
    'Series VSC: dq load-voltage PI through the series transformer winding\n' ...
    'Test: grid sag 220 -> 176 Vrms at 0.15 s, recovery at 0.30 s']));
note.Position = [240 15 650 65];
set_param(mdl, 'ModelBrowserVisibility', 'on');
end

function sig(sys, src, srcPort, dst, dstPort)
add_line(sys, [src '/' num2str(srcPort)], [dst '/' num2str(dstPort)], 'autorouting', 'on');
end

function sig_h(~, srcHandle, dstHandle)
add_line(get_param(get_param(srcHandle, 'Parent'), 'Parent'), srcHandle, dstHandle, 'autorouting', 'on');
end

function h = out_h(mdl, block, idx)
if nargin < 3
    idx = 1;
end
p = get_param([mdl '/' block], 'PortHandles');
h = p.Outport(idx);
end

function h = in_h(mdl, block, idx)
p = get_param([mdl '/' block], 'PortHandles');
h = p.Inport(idx);
end

function phys(mdl, a, fieldA, idxA, b, fieldB, idxB)
pa = get_param([mdl '/' a], 'PortHandles');
pb = get_param([mdl '/' b], 'PortHandles');
add_line(mdl, pa.(fieldA)(idxA), pb.(fieldB)(idxB), 'autorouting', 'on');
end
