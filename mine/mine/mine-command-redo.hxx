#pragma once

#include <mine/mine-command-base.hxx>

namespace mine
{
  class redo_command : public command
  {
  public:
    virtual editor_state
    execute (const editor_state& s) const override
    {
      // Meta-command: implementation is bypassed by editor_core::dispatch.
      //
      return s;
    }

    virtual std::string_view
    name () const noexcept override
    {
      return "redo";
    }

    virtual bool
    modifies_buffer () const noexcept override
    {
      return false;
    }
  };
}
