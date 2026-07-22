function metrics = verify_single_phase_upfc()
%VERIFY_SINGLE_PHASE_UPFC Run the model and check both UPFC objectives.

root = fileparts(mfilename('fullpath'));
mdl = 'SinglePhase_UPFC_Switching';
load_system(fullfile(root, [mdl '.slx']));
out = sim(mdl);

vdc = out.upfc_vdc;
vout = out.upfc_vout_rms;
vpcc = out.upfc_vpcc_rms;
qsh = out.upfc_qsh;
qref = out.upfc_qref;

pre = [0.10 0.14];
sag = [0.24 0.28];
post = [0.48 0.58];
metrics = struct( ...
    'Vout_pre_V', window_mean(vout, pre), ...
    'Vpcc_sag_V', window_mean(vpcc, sag), ...
    'Vout_sag_V', window_mean(vout, sag), ...
    'Vout_post_V', window_mean(vout, post), ...
    'Vdc_post_V', window_mean(vdc, post), ...
    'Qsh_post_var', window_mean(qsh, post), ...
    'Qref_post_var', window_mean(qref, post));

fprintf('Pre-sag output RMS:  %.3f V\n', metrics.Vout_pre_V);
fprintf('Sag PCC RMS:         %.3f V\n', metrics.Vpcc_sag_V);
fprintf('Sag output RMS:      %.3f V\n', metrics.Vout_sag_V);
fprintf('Post-sag output RMS: %.3f V\n', metrics.Vout_post_V);
fprintf('Post-sag DC link:    %.3f V\n', metrics.Vdc_post_V);
fprintf('Post-step shunt Q:   %.3f var\n', metrics.Qsh_post_var);

assert(abs(metrics.Vout_pre_V - 220) < 2, 'Pre-sag AC regulation failed.');
assert(metrics.Vpcc_sag_V < 190, 'The grid sag test did not occur.');
assert(abs(metrics.Vout_sag_V - 220) < 2, 'Series AC regulation failed during sag.');
assert(abs(metrics.Vout_post_V - 220) < 2, 'Post-sag AC regulation failed.');
assert(abs(metrics.Vdc_post_V - 400) < 2, 'DC-link regulation failed.');
assert(abs(metrics.Qsh_post_var - metrics.Qref_post_var) < 25, ...
    'Shunt reactive-power tracking failed.');

fig = figure('Color', 'w', 'Position', [100 100 980 720]);
tiledlayout(3, 1, 'TileSpacing', 'compact', 'Padding', 'compact');
nexttile;
plot(vpcc.time, vpcc.signals.values, 'Color', [0.35 0.35 0.35], 'LineWidth', 1.0);
hold on;
plot(vout.time, vout.signals.values, 'b', 'LineWidth', 1.2);
yline(220, 'r--', 'LineWidth', 1.0);
grid on; xlim([0 0.6]); ylabel('RMS voltage (V)');
legend('PCC', 'Regulated output', 'Reference', 'Location', 'best');
title('Single-phase UPFC switching-model verification');

nexttile;
plot(vdc.time, vdc.signals.values, 'Color', [0 0.55 0.25], 'LineWidth', 1.2);
hold on; yline(400, 'r--', 'LineWidth', 1.0);
grid on; xlim([0 0.6]); ylabel('DC link (V)');
legend('Vdc', 'Reference', 'Location', 'best');

nexttile;
plot(qsh.time, qsh.signals.values, 'Color', [0.75 0.2 0.1], 'LineWidth', 1.1);
hold on;
plot(qref.time, qref.signals.values, 'k--', 'LineWidth', 1.0);
grid on; xlim([0 0.6]); xlabel('Time (s)'); ylabel('Reactive power (var)');
legend('Measured Q', 'Reference', 'Location', 'best');

exportgraphics(fig, fullfile(root, 'UPFC_verification.png'), 'Resolution', 180);
save(fullfile(root, 'UPFC_verification.mat'), 'metrics');
close(fig);
close_system(mdl, 0);
end

function value = window_mean(signal, window)
idx = signal.time >= window(1) & signal.time <= window(2);
value = mean(signal.signals.values(idx));
end
