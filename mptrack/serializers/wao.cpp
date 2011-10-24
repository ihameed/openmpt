#include "stdafx.h"

#include "wao.h"

#include "../pervasives/pervasives.h"

#include "../../soundlib/patternContainer.h"
#include "../../soundlib/Sndfile.h"

#include <WDL/filename.h>
#include <WDL/fileread.h>
#include <WDL/filewrite.h>

#include <json/json.h>

#include <sstream>
#include <string>


using namespace modplug::pervasives;

namespace modplug {
namespace serializers {


bool _wao_write_envelope(Json::Value &out, const modplug::tracker::modenvelope_t &envelope) {
    out["loop_start"] = envelope.loop_start;
    out["loop_end"]   = envelope.loop_end;

    out["sustain_start"] = envelope.sustain_start;
    out["sustain_end"]   = envelope.sustain_end;
    out["release_node"]  = envelope.release_node;

    Json::Value &nodes = out["nodes"];
    for (size_t nodeidx = 0; nodeidx < envelope.num_nodes; ++nodeidx) {
        Json::Value &node = nodes.append(Json::Value());
        node["tick"]  = envelope.Ticks[nodeidx];
        node["value"] = envelope.Values[nodeidx];
    }
    return true;
}

bool _wao_write_instrument(Json::Value &out, const modplug::tracker::modinstrument_t &instrument) {
    out["name"]            = std::string(instrument.name,     32);
    out["legacy_filename"] = std::string(instrument.legacy_filename, 32);
    out["fadeout"]         = instrument.fadeout;
    out["global_volume"]   = instrument.global_volume;
    out["default_pan"]     = instrument.default_pan;

    _wao_write_envelope(out["volume_envelope"],  instrument.volume_envelope);
    _wao_write_envelope(out["panning_envelope"], instrument.panning_envelope);
    _wao_write_envelope(out["pitch_envelope"],   instrument.pitch_envelope);

    out["note_map"];
    out["sample_map"];

    out["new_note_action"]       = instrument.new_note_action;
    out["duplicate_check_type"]  = instrument.duplicate_check_type;
    out["duplicate_note_action"] = instrument.duplicate_note_action;

    out["random_pan_weight"]    = instrument.random_pan_weight;
    out["random_volume_weight"] = instrument.random_volume_weight;

    out["default_filter_cutoff"]    = instrument.default_filter_cutoff & 0x7f;
    out["default_filter_resonance"] = instrument.default_filter_resonance & 0x7f;
    out["default_filter_cutoff_enabled"]    = static_cast<bool>(instrument.default_filter_cutoff & 0x80);
    out["default_filter_resonance_enabled"] = static_cast<bool>(instrument.default_filter_resonance & 0x80);

    out["midi_bank"]     = instrument.midi_bank;
    out["midi_program"]  = instrument.midi_program;
    out["midi_channel"]  = instrument.midi_channel;
    out["midi_drum_set"] = instrument.midi_drum_set;

    out["pitch_pan_separation"] = instrument.pitch_pan_separation;
    out["pitch_pan_center"]     = instrument.pitch_pan_center;

    out["graph_insert"] = instrument.graph_insert;

    out["volume_ramp_up"]   = instrument.volume_ramp_up;
    out["volume_ramp_down"] = instrument.volume_ramp_down;

    out["random_cutoff_weight"]    = instrument.random_cutoff_weight;
    out["random_resonance_weight"] = instrument.random_resonance_weight;

    out["default_filter_mode"] = instrument.default_filter_mode;

    out["pitch_to_tempo_lock"] = instrument.pitch_to_tempo_lock;

    out["tuning"];
    return true;
}

bool _wao_write_sample(Json::Value &out, const modplug::tracker::modsample_t &sample) {
    out["name"] = "test";
    out["legacy_filename"] = sample.legacy_filename;
    out["length"]          = sample.length;
    out["loop_start"]      = sample.loop_start;
    out["loop_end"]        = sample.loop_end;
    out["sustain_start"]   = sample.sustain_start;
    out["sustain_end"]     = sample.sustain_end;
    out["c5_samplerate"]   = sample.c5_samplerate;
    out["pan"]             = sample.default_pan;
    out["volume"]          = sample.default_volume;
    out["global_volume"]   = sample.global_volume;
    out["vibrato_type"]    = sample.vibrato_type;
    out["vibrato_sweep"]   = sample.vibrato_sweep;
    out["vibrato_depth"]   = sample.vibrato_depth;
    out["vibrato_rate"]    = sample.vibrato_rate;
    return true;
}

bool _wao_write_pattern(Json::Value &out, const MODPATTERN &pattern) {
    size_t num_rows     = pattern.GetNumRows();
    size_t num_channels = pattern.GetNumChannels();

    out["name"] = pattern.GetName().GetBuffer();
    out["rows"] = num_rows;
    out["rows_per_beat"]    = pattern.GetRowsPerBeat();
    out["rows_per_measure"] = pattern.GetRowsPerMeasure();

    Json::Value &pattern_data = out["data"];
    for (size_t rowidx = 0; rowidx < num_rows; ++rowidx) {
        const modplug::tracker::modcommand_t *command = pattern.GetpModCommand(rowidx, 0);
        if (!command) break;

        for (size_t chnidx = 0; chnidx < num_channels; ++chnidx) {
            if (command->IsEmpty()) continue;
            Json::Value &note = pattern_data.append(Json::Value());
            note["row"]     = rowidx;
            note["channel"] = chnidx;
            note["note"]    = command->note;
            note["instr"]   = command->instr;
            note["volcmd"]  = command->volcmd;
            note["volval"]  = command->vol;
            note["fxcmd"]   = command->command;
            note["fxparam"] = command->param;
            ++command;
        }
    }
    return true;
}

bool write_wao(CPatternContainer &pattern_list, CSoundFile &kitchen_sink) {
    Json::Value root;

    root["version"] = 1;
    root["name"] = "epanos";
    Json::Value &message     = root["message"];
    Json::Value &orderlist   = root["orderlist"];
    Json::Value &patterns    = root["patterns"];
    Json::Value &samples     = root["samples"];
    Json::Value &instruments = root["instruments"];
    Json::Value &graphstate  = root["graphstate"];
    Json::Value &pluginstate = root["pluginstate"];

    for (size_t patternidx = 0; patternidx < pattern_list.Size(); ++patternidx) {
        MODPATTERN &pattern = pattern_list[patternidx];
        Json::Value &pattern_json = patterns.append(Json::Value());
        _wao_write_pattern(pattern_json, pattern);
    }

    size_t num_samples = kitchen_sink.m_nSamples;
    for (size_t sampleidx = 1; sampleidx < num_samples; ++sampleidx) {
        Json::Value &sample_json = samples.append(Json::Value());
        modplug::tracker::modsample_t &sample = kitchen_sink.Samples[sampleidx];
        _wao_write_sample(sample_json, sample);
    }

    size_t num_instruments = kitchen_sink.m_nInstruments;
    for (size_t instrumentidx = 1; instrumentidx < num_instruments; ++instrumentidx) {
        Json::Value &instrument_json = instruments.append(Json::Value());
        modplug::tracker::modinstrument_t *instrument = kitchen_sink.Instruments[instrumentidx];
        if (!instrument) continue;
        _wao_write_instrument(instrument_json, *instrument);
    }

    std::string buf = root.toStyledString();

    WDL_FileWrite outfile("D:\\myhompis.wao");
    outfile.Write(buf.c_str(), buf.size());
    outfile.SyncOutput(true);

    return false;
}

bool read_wao(CPatternContainer &pattern_list, CSoundFile &kitchen_sink) {
    return false;
}


}
}
