describe("PushStream", function() {

  beforeEach(function() {
  });

  it("should use default port", function() {
    expect(PushStream.port).toEqual(80);
  });

});
