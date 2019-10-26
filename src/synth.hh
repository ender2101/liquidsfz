/*
 * liquidsfz - sfz support using fluidsynth
 *
 * Copyright (C) 2019  Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

struct Synth
{
  static constexpr double VOLUME_HEADROOM_DB = 12;

  std::minstd_rand random_gen;
  jack_port_t *midi_input_port = nullptr;
  fluid_synth_t *synth = nullptr;
  fluid_sfont_t *fake_sfont = nullptr;
  fluid_preset_t *fake_preset = nullptr;
  unsigned int next_id = 0;
  Loader sfz_loader;

  fluid_voice_t *
  alloc_voice_with_id (fluid_sample_t *flsample, int chan, int key, int vel)
  {
    struct Data {
      fluid_sample_t *flsample = nullptr;
      fluid_voice_t *flvoice = nullptr;
    } data;
    if (!fake_preset)
      {
        fake_sfont = new_fluid_sfont (
          [](fluid_sfont_t *) { return ""; },
          [](fluid_sfont_t *sfont, int bank, int prenum) { return (fluid_preset_t *) nullptr; },
          nullptr, nullptr,
          [](fluid_sfont_t *) { return int(0); });

        auto note_on_lambda = [](fluid_preset_t *preset, fluid_synth_t *synth, int chan, int key, int vel)
          {
            Data *data = (Data *) fluid_preset_get_data (preset);
            data->flvoice = fluid_synth_alloc_voice (synth, data->flsample, chan, key, vel);
            return 0;
          };
        fake_preset = new_fluid_preset (fake_sfont,
          [](fluid_preset_t *) { return "preset"; },
          [](fluid_preset_t *) { return 0; },
          [](fluid_preset_t *) { return 0; },
          note_on_lambda,
          [](fluid_preset_t *) { });
      }
    data.flsample = flsample;
    fluid_preset_set_data (fake_preset, &data);
    fluid_synth_start (synth, next_id++, fake_preset, 0, chan, key, vel);
    return data.flvoice;
  }
  float
  env_time2gen (float time_sec)
  {
    return 1200 * log2f (std::clamp (time_sec, 0.001f, 100.f));
  }
  float
  env_level2gen (float level_perc)
  {
    return -10 * 20 * log10 (std::clamp (level_perc, 0.001f, 100.f) / 100);
  }
  double
  velocity_track_db (const Region& r, int midi_velocity)
  {
    double curve = (midi_velocity * midi_velocity) / (127.0 * 127.0);
    double veltrack_factor = r.amp_veltrack * 0.01;

    double offset = (veltrack_factor >= 0) ? 1 : 0;
    double v = (offset - veltrack_factor) + veltrack_factor * curve;

    return db_from_factor (v, -144);
  }
  struct Voice
  {
    int channel = 0;
    int key = 0;
    int id = -1;
    fluid_voice_t *flvoice = nullptr;
    Region *region = nullptr;
  };
  std::vector<Voice> voices;
  void
  add_voice (Region *region, int channel, int key, fluid_voice_t *flvoice)
  {
    assert (flvoice);

    Voice *voice = nullptr;
    for (auto& v : voices)
      if (!v.flvoice)
        {
          /* overwrite old voice in vector */
          voice = &v;
          break;
        }
    if (!voice)
      {
        voice = &voices.emplace_back();
      }
    voice->region = region;
    voice->channel = channel;
    voice->key = key;
    voice->id = fluid_voice_get_id (flvoice);
    voice->flvoice = flvoice;
  }
  void
  cleanup_finished_voice (fluid_voice_t *flvoice)
  {
    for (auto& voice : voices)
      {
        if (flvoice == voice.flvoice)
          voice.flvoice = nullptr;
      }
  }
  void
  note_on (int chan, int key, int vel)
  {
    // - random must be >= 0.0
    // - random must be <  1.0  (and never 1.0)
    double random = random_gen() / double (random_gen.max() + 1);

    for (auto& region : sfz_loader.regions)
      {
        if (region.lokey <= key && region.hikey >= key &&
            region.lovel <= vel && region.hivel >= vel &&
            region.trigger == Trigger::ATTACK)
          {
            bool cc_match = true;
            for (size_t cc = 0; cc < region.locc.size(); cc++)
              {
                if (region.locc[cc] != 0 || region.hicc[cc] != 127)
                  {
                    int val;
                    if (fluid_synth_get_cc (synth, chan, cc, &val) == FLUID_OK)
                      {
                        if (val < region.locc[cc] || val > region.hicc[cc])
                          cc_match = false;
                      }
                  }
              }
            if (!cc_match)
              continue;
            if (region.play_seq == region.seq_position)
              {
                /* in order to make sequences and random play nice together
                 * random check needs to be done inside sequence check
                 */
                if (region.lorand <= random && region.hirand > random)
                  {
                    if (region.cached_sample)
                      {
                        for (size_t ch = 0; ch < region.cached_sample->flsamples.size(); ch++)
                          {
                            auto flsample = region.cached_sample->flsamples[ch];

                            printf ("%d %s#%zd\n", key, region.sample.c_str(), ch);
                            fluid_sample_set_pitch (flsample, region.pitch_keycenter, 0);

                            // auto flvoice = fluid_synth_alloc_voice (synth, flsample, chan, key, vel);
                            /* note: we handle velocity based volume (amp_veltrack) ourselves:
                             *    -> we play all notes via fluidsynth at maximum velocity */
                            auto flvoice = alloc_voice_with_id (flsample, chan, key, 127);
                            cleanup_finished_voice (flvoice);

                            if (region.loop_mode == LoopMode::SUSTAIN || region.loop_mode == LoopMode::CONTINUOUS)
                              {
                                fluid_sample_set_loop (flsample, region.loop_start, region.loop_end);
                                fluid_voice_gen_set (flvoice, GEN_SAMPLEMODE, 1);
                              }

                            if (region.cached_sample->flsamples.size() == 2) /* pan stereo voices */
                              {
                                if (ch == 0)
                                  fluid_voice_gen_set (flvoice, GEN_PAN, -500);
                                else
                                  fluid_voice_gen_set (flvoice, GEN_PAN, 500);
                              }
                            /* volume envelope */
                            fluid_voice_gen_set (flvoice, GEN_VOLENVDELAY, env_time2gen (region.ampeg_delay));
                            fluid_voice_gen_set (flvoice, GEN_VOLENVATTACK, env_time2gen (region.ampeg_attack));
                            fluid_voice_gen_set (flvoice, GEN_VOLENVDECAY, env_time2gen (region.ampeg_decay));
                            fluid_voice_gen_set (flvoice, GEN_VOLENVSUSTAIN, env_level2gen (region.ampeg_sustain));
                            fluid_voice_gen_set (flvoice, GEN_VOLENVRELEASE, env_time2gen (region.ampeg_release));

                            /* volume */
                            double attenuation = -10 * (region.volume - VOLUME_HEADROOM_DB + velocity_track_db (region, vel));
                            fluid_voice_gen_set (flvoice, GEN_ATTENUATION, attenuation);

                            fluid_synth_start_voice (synth, flvoice);
                            add_voice (&region, chan, key, flvoice);
                          }
                      }
                  }
              }
            region.play_seq++;
            if (region.play_seq > region.seq_length)
              region.play_seq = 1;
          }
      }
  }
  void
  note_off (int chan, int key)
  {
    for (auto& voice : voices)
      {
        if (voice.flvoice && voice.channel == chan && voice.key == key && voice.region->loop_mode != LoopMode::ONE_SHOT)
          {
            // fluid_synth_release_voice (synth, voice.flvoice);
            fluid_synth_stop (synth, voice.id);
            voice.flvoice = nullptr;
          }
      }
  }
  struct
  MidiEvent
  {
    uint offset;
    unsigned char midi_data[3];
  };
  std::vector<MidiEvent> midi_events;
  void
  add_midi_event (uint offset, unsigned char *midi_data)
  {
    unsigned char status = midi_data[0] & 0xf0;
    if (status == 0x80 || status == 0x90)
      {
        MidiEvent event;
        event.offset = offset;
        std::copy_n (midi_data, 3, event.midi_data);
        midi_events.push_back (event);
      }
  }
  int
  process (float **outputs, jack_nframes_t nframes)
  {
    for (const auto& midi_event : midi_events)
      {
        unsigned char status  = midi_event.midi_data[0] & 0xf0;
        unsigned char channel = midi_event.midi_data[0] & 0x0f;
        if (status == 0x80)
          {
            // printf ("note off, ch = %d\n", channel);
            note_off (channel, midi_event.midi_data[1]);
          }
        else if (status == 0x90)
          {
            // printf ("note on, ch = %d, note = %d, vel = %d\n", channel, in_event.buffer[1], in_event.buffer[2]);
            note_on (channel, midi_event.midi_data[1], midi_event.midi_data[2]);
          }
      }
    std::fill_n (outputs[0], nframes, 0.0);
    std::fill_n (outputs[1], nframes, 0.0);
    fluid_synth_process (synth, nframes,
                         0, nullptr, /* no effects */
                         2, outputs);
    midi_events.clear();
    return 0;
  }
};

