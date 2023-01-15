# frozen_string_literal: true

module Panko
  class Association
    def duplicate
      Panko::Association.new(
        name_sym,
        name_str,
        Panko::SerializationDescriptor.duplicate(descriptor),
        link_func_sym
      )
    end

    def inspect
      "<Panko::Association name=#{name_str.inspect}>"
    end
  end
end
