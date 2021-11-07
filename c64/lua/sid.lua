
local bit = require("bit")

local module = {}

-- Noise generation

local function lshift32(val, n)
  return bit.band(bit.lshift(val, n), 0xFFFFFFFF)
end

local function NSHIFT(v, n)
  return bit.bor(
    lshift32(v, n),
    bit.band(
      bit.bxor(
        bit.rshift(v, 23 - n),
        bit.rshift(v, 18 - n)),
      lshift32(1, n) - 1))
end

local function NVALUE(v)
  return bit.bor(
    noiseLSB[bit.band(v, 0xFF)],
    bit.bor(
      noiseMID[bit.band(bit.rshift(v, 8), 0xFF)],
      noiseMSB[bit.band(bit.rshift(v, 16), 0xFF)]))
end

local NSEED = 0x7ffff8
local noiseArray = ffi.typeof("uint8_t[?]")

-- Noise tables
local NOISE_TABLE_SIZE 256
local noiseLSB = noiseArray(NOISE_TABLE_SIZE)
local noiseMID = noiseArray(NOISE_TABLE_SIZE)
local noiseMSB = noiseArray(NOISE_TABLE_SIZE)

local adrtable = ffi.new("uint16_t[?]", 16)
local adrtableValues = {
  1, 4, 8, 12, 19, 28, 34, 40, 50, 125, 250, 400, 500, 1500, 2500, 4000
}
for i, x in ipairs(adrtableValues) do
  adrtable[i] = x
end

local exptable = {
  0x30000000, 0x1c000000, 0x0e000000, 0x08000000, 0x04000000, 0x00000000
}

local sidReadClocks = {}

local lowPassParam = {}
local bandPassParam = {}
local filterResTable = {}
local filterRefFreq = 44100.0
local ampMod1x8 = {}

local buf
local blen = 0

local BitMask_Volume = 0x0F

local BitFlag_FilterType_LowPass = bit.lshift(1, 4)
local BitFlag_FilterType_BandPass = bit.lshift(1, 5)
local BitFlag_FilterType_HighPass = bit.lshift(1, 6)
local BitFlag_FilterType_Voice3Disabled = bit.lshift(1, 7)

local BitFlag_FilterType_LowPassAndBandPass =
  BitFlag_FilterType_LowPass + BitFlag_FilterType_BandPass
local BitFlag_FilterType_LowPassAndHighPass =
  BitFlag_FilterType_LowPass + BitFlag_FilterType_HighPass
local BitFlag_FilterType_BandPassAndHighPass =
  BitFlag_FilterType_BandPass + BitFlag_FilterType_HighPass
local BitFlag_FilterType_All =
  BitFlag_FilterType_LowPassAndBandPass + BitFlag_FilterType_HighPass

local function sidFilter(voice)
  if not voice.filter then
    return
  end

  if voice.s.filterType == 0 then

    voice.filtIO = 0

  else

    if voice.s.filterType == BitFlag_FilterType_BandPass then

      voice.filtLow = voice.filtLow + (voice.filtRef * voice.s.filterDy)
      local filtRefAdd = (
        (voice.filtIO - voice.filtLow - (voice.filtRef * voice.filterResDy))
        * voice.s.filterDy)
      voice.filtRef = voice.filtRef + filtRefAdd
      voice.filtIO = voice.filtRef - math.floor(voice.filtLow / 4)

    elseif voice.s.filterType == BitFlag_FilterType_HighPass then

      voice.filtLow = voice.filtLow + (voice.filtRef * voice.s.filterDy * 0.1)
      voice.filtRef = voice.filtRef + (
        voice.filtIO - voice.filtLow
        - (voice.filtRef * voice.s.filterResDy)) * voice.s.filterDy
      local sample = voice.filtRef - voice.filtIO / 8
      if sample < -128 then
        sample = -128
      elseif sample > 127 then
        sample = 127
      end
      voice.filtIO = sample

    else

      voice.filtLow = voice.filtLow + (voice.filtRef * voice.s.filterDy)
      local sample = voice.filtIO
      local sample2 = sample - voice.filtLow
      local tmp = sample2
      local sample2 = sample2 - voice.filtRef * voice.s.filterResDy
      voice.filtRef = voice.filtRef + (sample2 * voice.s.filterDy)

      if voice.s.filterType == BitFlag_FilterType_LowPass then
        voice.filtIO = voice.filtLow
      elseif voice.s.filterType == BitFlag_FilterType_LowPassAndBandPass then
        voice.filtIO = voice.filtLow -- XXX: IS THIS RIGHT?
      elseif voice.s.filterType == BitFlag_FilterType_LowPassAndHighPass then
        voice.filtIO = sample - bit.rshift(tmp, 1)
      elseif voice.s.filterType == BitFlag_FilterType_BandPassAndHighPass then
        voice.filtIO = tmp
      elseif voice.s.filterType == BitFlag_FilterType_All then
        voice.filtIO = sample - bit.rshift(tmp, 1) -- XXX: IS THIS RIGHT?
      else
        voice.filtIO = 0
      end

    end
  end
end

local function oscillate(pv)

  local f = pv.f

  if pv.fm == BitFlag_Waveform_PulseSawtooth then goto PulseSawtooth end
  if pv.fm == BitFlag_Waveform_Sawtooth then goto Sawtooth end
  if pv.fm == BitFlag_Waveform_Ring then goto Ring end
  if pv.fm == BitFlag_Waveform_Triangle then goto Triangle end
  if pv.fm == BitFlag_Waveform_PulseTriangle then goto PulseTriangle end
  if pv.fm == BitFlag_Waveform_Noise then goto Noise end
  if pv.fm == BitFlag_Waveform_Pulse then goto Pulse end

  ::PulseSawtooth::
  if f <= pv.pw then
    return 0
  end
  ::Sawtooth::
  return bit.rshift(f, 17)

  ::Ring::
  f = bit.bxor(f, bit.band(pv.vprev.f, 0x80000000))
  ::Triangle::
  if f < 0x80000000 then
    return bit.rshift(f, 16)
  end
  return 0xFFFF - bit.rshift(f, 16)

  ::PulseTriangle::
  if f <= pv.pw then
    return 0
  elseif f < 0x80000000 then
    return bit.rshift(f, 16)
  end
  return 0xFFFF - bit.rshift(f, 16)

  ::Noise::
  return bit.lshift(NVALUE(NSHIFT(pv.rv, bit.rshift(pv.f, 28))), 7)

  ::Pulse::
  if f >= pv.pw then
    return 0x7FFF
  end

  return 0

end

local function setEnvelope(voice, fm)
  local i

  ::ChangeFm::

  if fm == BitFlag_Envelope_Attack then

    voice.adsrs = voice.s.adrs[voice.attack]
    voice.adsrs = 0

  elseif fm == BitFlag_Envelope_Decay then

    -- XXX: VICE says fix this
    if voice.adsr <= voice.s.sz[voice.sustain] then
      fm = BitFlag_Envelope_Sustain
      goto ChangeFm
    end
    i = 0
    while voice.adsr < exptable[i] do
      i = i + 1
    end
    voice.adsrs = bit.rshift(0 - voice.s.adrs[voice.decay], i)
    voice.adsrs = voice.s.sz[voice.sustain]
    if exptable[i] > voice.adsrs then
      voice.adsrs = exptable[i]
    end

  elseif fm == BitFlag_Envelope_Sustain then

    if voice.adsr > voice.s.sz[voice.sustain] then
      fm = BitFlag_Envelope_Decay
      goto ChangeFm
    end
    voice.adsrs = 0
    voice.adsrz = 0

  elseif fm == BitFlag_Envelope_Release then

    if voice.adsr == 0 then
      fm = BitFlag_Envelope_Idle
      goto ChangeFm
    end
    i = 0
    while voice.adsr < exptable[i] do
      i = i + 1
    end
    voice.adsrs = bit.rshift(0 - voice.s.adrs[voice.release], i)
    voice.adsrz = exptable[i]

  elseif fm == BitFlag_Envelope_Idle then

    voice.adsrs = 0
    voice.adsrz = 0

  else

    error("Invalid envelope state.")

  end

  voice.adsrm = fm

end

local function triggerEnvelope(voice)
  if voice.adsrm == BitFlag_Envelope_Attack then
    voice.adsr = 0x7fffffff
    setEnvelope(voice, BitFlag_Envelope_Decay)
  elseif voice.adsrm == BitFlag_Envelope_Decay
      or voice.adsrm == BitFlag_Envelope_Release
  then
    if voice.adsr >= 0x80000000 then
      voice.adsr = 0
    end
    setEnvelope(voice, voice.adsrm)
  end
end

local function setupVoice(voice)
  if not voice.update then
    return
  end

  voice.attack = voice.d[5] / 0x10
  voice.decay = voice.d[5] % 0x10
  voice.sustain = voice.d[6] / 0x10
  voice.release = voice.d[6] % 0x10

  if voice.d[4] % 4 > 0 then
    voice.sync = 1
  else
    voice.sync = 0
  end

  voice.fs = voice.s.speed1 * (voice.d[0] + voice.d[1] * 0x100)

  if bit.band(voice.d[4], 0x08) != 0 then
    -- the reset bit is on
    voice.fm = BitFlag_Waveform_Test
    voice.pw = 0
    voice.f = 0
    voice.fs = 0
    voice.rv = NSEED
  else
    local waveform = bit.rshift(bit.band(voice.d[4], 0xF0), 4)
    local fm
    if waveform == 1 then
      if bit.band(voice.d[4], 4) != 0 then
        fm = BitFlag_Waveform_Ring
      else
        fm = BitFlag_Waveform_Triangle
      end
    else
      fm = {
        [0] = BitFlag_Waveform_None,
        [2] = BitFlag_Waveform_Sawtooth,
        [4] = BitFlag_Waveform_Pulse,
        [5] = BitFlag_Waveform_PulseTriangle,
        [6] = BitFlag_Waveform_PulseSawtooth,
        [8] = BitFlag_Waveform_Noise,
      }[waveform]
      if not fm then
        fm = BitFlag_Waveform_None
      end
    end
  end

  local env
  local lowBitSet = ( pv.d[4] % 2 != 0)
  if voice.adsrm == BitFlag_Envelope_Attack
    or voice.adsrm == BitFlag_Envelope_Decay
    or voice.adsrm == BitFlag_Envelope_Sustain
  then
    if lowBitSet then
      if voice.gateFlip != 0 then
        env = BitFlag_Envelope_Attack
      else
        env = BitFlag_Envelope_Release
      end
    else
      env = voice.adsrm
    end
  else
    if lowBitSet then
      env = BitFlag_Envelope_Attack
    else
      env = voice.adsrm
    end
  end
  setEnvelope(voice, env)

  voice.update = 0
  voice.gateFlip = 0

end

local function calculateSingleSample(sound)
  setupSid(sound)
  for i = 0 to 2 do
    setupVoice(sound.v[i])
  end
  local v = sound.v
  local v0 = v[0]
  local v1 = v[1]
  local v2 = v[2]

  -- addfptrs, noise & hard sync test

  local dosync = {}

  for i = 0 to 2 do
    local nxtI = (i + 1) % 3
    v[i].f = v[i].f + v[i].fs
    if v[i].f < v[i].fs then
      v[i].rv = NSHIFT(v[i].rv, 16)
      if v[nxtI].sync then
        dosync[nxtI] = true
      end
    end
  end

  for i = 2 to 0 step -1 do
    if dosync[i] then
      v[i].rv = NSHIFT(v[i].rv, bit.rshift(v[i].f, 28))
      v[i].f = 0
    end
  end

  for i = 0 to 2 do
    v[i].adsr = v[i].adsrs
    if (v[i].adsr + 0x80000000) % 0x100000000
      < (v[i].adsrz + 0x80000000) % 0x100000000
    then
      triggerEnvelope(v[i])
    end
  end

  local osc = {}
  for i = 0 to 2 do
    local oscillation = v[i].adsr / (2^16)
    if oscillation != 0 then
      if i != 2 or sound.has3 then
        oscillation = oscillate(v[i])
      else
        oscillation = 0
      end
    end
    osc[i] = oscillation
  end

  if sound.emulateFilter then
    for i = 0 to 2 do
      v[i].filtIO = ampMod1x8[osc[i] / (2^22)]
      doFilter(v[i])
      osc[i] = (v[i].filtIO + 0x80) * (2 ^ (7 + 15))
    end
  end

  local sum = osc[0] + osc[1] + osc[2]
  local reduced = (sum / (2^20)) - 0x600
  return reduced * sound.vol
end

local function calculateSamples(sound, sampleBuffer, nr, interleave, delta_t)
  local count
  if sound.factor == 1000 then
    count = nr
  else
    count = nr * sound.factor / 1000
  end
  for i = 0, count - 1 do
    sampleBuffer[i * interleave] = calculateSingleSample(sound)
  end
  return nr
end

local function init(sound, speed, cycles_per_sec, factor)
  sound.factor = factor
  sound.speed1 = (256 * cycles_per_sec) / speed
  for i = 0 to 15 do
    sound.adrs[i] = 500 * 8 * sound.speed1 / adrtable[i]
    sound.sz[i] = 0x8888888 * i
  end
  sound.update = 1
  -- TODO: resources_get_int("SidFilters", &(psid->emulateFilter))
  sound.emulateFilter = true -- assume true for now
  initFilter(sound)
  for i = 0 to 2 do
    sound.v[i] = {}
  end
  for i = 0 to 2 do
    local v = sound.v[i]
    v.vprev = sound.v[(i + 2) % 3]
    v.vnext = sound.v[(i + 1) % 3]
    v.nr = i
    v.d = sound.d + i * 7
    v.s = sound
    v.rv = NSEED
    v.filtLow = 0
    v.filtRef = 0
    v.filtIO = 0
    v.update = 1
    setupVoice(v)
  end
  for i = 0 to NOISE_TABLE_SIZE do
    noiseLSB[i] = bit.bor(
  end
end

return module

