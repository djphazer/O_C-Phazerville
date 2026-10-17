class PassthruApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return " - ";
  }
  void Start() override {}
  void Controller() override {}
  void View() override {}
  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {}

  // these won't actually work right...
  AudioStream_F32& InputStream() override {
    return passthru;
  }
  AudioStream_F32& OutputStream() override {
    return passthru;
  }
  // float only
  AudioStream_F32& InputF32() override {
    return passthru;
  }
  AudioStream_F32& OutputF32() override {
    return passthru;
  }

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<STEREO> passthru16;
  AudioPassthroughF32<STEREO> passthru;
};
