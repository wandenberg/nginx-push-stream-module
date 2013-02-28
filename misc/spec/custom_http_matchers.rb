module CustomHttpMatchers
  class BeHttpStatus
    def initialize(expected)
      @expected = expected
      @should_has_content = nil
    end

    def matches?(target)
      @target = target
      ret = @target.response_header.status.eql?(@expected)
      ret = @should_has_content ? has_content? : !has_content? unless (@should_has_content.nil? || !ret)
      ret
    end
    alias == matches?

    def without_body
      @should_has_content = false
      self
    end

    def with_body
      @should_has_content = true
      self
    end

    def failure_message_for_should
      "expected that the #{@target.req.method} to #{@target.req.uri} to #{description}"
    end

    def failure_message_for_should_not
      "expected that the #{@target.req.method} to #{@target.req.uri} not to #{description}"
    end

    def description
      returned_values = " but returned with status #{@target.response_header.status} and content_length equals to #{@target.response_header.content_length}"
      about_content = " and #{@should_has_content ? "with body" : "without body"}" unless @should_has_content.nil?
      "be returned with status #{@expected}#{about_content}#{returned_values}"
    end

    private
    def has_content?
      @target.response_header.content_length > 0
    end
  end

  def be_http_status(expected)
    BeHttpStatus.new(expected)
  end
end
