#include <libmine/window/gtk.hxx>

namespace mine
{
  gtk::
  gtk () noexcept
    : app_ (nullptr),
      win_ (nullptr),
      gl_area_ (nullptr)
  {
    // Must be called before any other Adwaita functions.
    //
    adw_init ();

    // Create application with dummy ID for DBus registration.
    //
    app_ = ADW_APPLICATION (
      adw_application_new ("org.mine.editor", G_APPLICATION_DEFAULT_FLAGS));

    if (app_ != nullptr)
      g_signal_connect (app_, "activate", G_CALLBACK (activate_cb), this);
  }

  gtk::
  ~gtk () noexcept
  {
    if (app_ != nullptr)
      g_object_unref (app_);
  }

  void gtk::
  run ()
  {
    if (app_ == nullptr)
      throw runtime_error ("failed to create GTK application");

    g_application_run (G_APPLICATION (app_), 0, nullptr);
  }

  void gtk::
  render ()
  {
    if (gl_area_ != nullptr)
      gtk_widget_queue_draw (GTK_WIDGET (gl_area_));
  }

  void gtk::
  activate_cb (GtkApplication* app, gpointer user_data)
  {
    auto self (static_cast<gtk*> (user_data));

    // Create main window.
    //
    self->win_ = ADW_APPLICATION_WINDOW (
      adw_application_window_new (GTK_APPLICATION (app)));

    gtk_window_set_title (GTK_WINDOW (self->win_), "MINE");
    gtk_window_set_default_size (GTK_WINDOW (self->win_), 800, 600);

    // Create GL area.
    //
    self->gl_area_ = GTK_GL_AREA (gtk_gl_area_new ());

    // Set up OpenGL context requirements.
    //
    gtk_gl_area_set_allowed_apis (
      self->gl_area_,
      static_cast<GdkGLAPI> (GDK_GL_API_GL | GDK_GL_API_GLES));
    gtk_gl_area_set_required_version (self->gl_area_, 2, 0);
    gtk_gl_area_set_has_depth_buffer (self->gl_area_, FALSE);
    gtk_gl_area_set_has_stencil_buffer (self->gl_area_, FALSE);
    gtk_gl_area_set_auto_render (self->gl_area_, TRUE);

    // Allow GL area to fill available space.
    //
    GtkWidget* gl_widget (GTK_WIDGET (self->gl_area_));
    gtk_widget_set_hexpand (gl_widget, TRUE);
    gtk_widget_set_vexpand (gl_widget, TRUE);

    // Connect GL context lifecycle handlers.
    //
    g_signal_connect (self->gl_area_,
                     "realize",
                     G_CALLBACK (realize_cb),
                     self);
    g_signal_connect (self->gl_area_,
                     "render",
                     G_CALLBACK (render_cb),
                     self);
    g_signal_connect (self->gl_area_,
                     "unrealize",
                     G_CALLBACK (unrealize_cb),
                     self);

    // Set window content.
    //
    adw_application_window_set_content (self->win_, gl_widget);

    // Show window.
    //
    gtk_window_present (GTK_WINDOW (self->win_));
  }

  void gtk::
  realize_cb (GtkGLArea* area, gpointer user_data)
  {
    // Make GL context current.
    //
    gtk_gl_area_make_current (area);

    if (GError* error = gtk_gl_area_get_error (area))
    {
      g_warning ("Failed to create GL context: %s\n", error->message);
      g_error_free (error);
      return;
    }
  }

  void gtk::
  render_cb (GtkGLArea* area, GdkGLContext* context, gpointer user_data)
  {
    auto self (static_cast<gtk*> (user_data));

    // Clear screen.
    //
    glClearColor (0.1f, 0.1f, 0.1f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);
  }

  void gtk::
  unrealize_cb (GtkGLArea* area, gpointer user_data)
  {
    gtk_gl_area_make_current (area);

    if (gtk_gl_area_get_error (area) != nullptr)
      return;

    // TODO: Cleanup GL resources
  }
}
