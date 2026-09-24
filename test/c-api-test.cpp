#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mapnik_c_api.h"
#include <fstream>

// https://github.com/philsquared/Catch/wiki/Supplying-your-own-main()
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "test/c-api-test-cfg.h"

TEST_CASE( "map/srs", "should get/set srs string" ) {
      mapnik_map_t * map;
      map = mapnik_map(256,256);
      const char *srs = mapnik_map_get_srs(map);
      REQUIRE( (0==strcmp(srs,"+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs") || 0==strcmp(srs,"epsg:4326")) );
      mapnik_map_set_srs(map,"+init=epsg:4326");
      const char *srs2 = mapnik_map_get_srs(map);
      REQUIRE( 0==strcmp(srs2,"+init=epsg:4326") );
      mapnik_map_free(map);
}

TEST_CASE( "map/load", "should load xml" ) {
      mapnik_map_t * map;
      map = mapnik_map(256,256);
      mapnik_register_datasources(MAPNIK_PLUGINDIR, NULL);
      REQUIRE_FALSE(mapnik_map_load(map,"sample/stylesheet.xml"));
      mapnik_map_free(map);
}

TEST_CASE( "map/load_string", "should load a string" ) {
    const char* minimal_map_string = "<?xml version=\"1.0\" encoding=\"utf-8\"?><Map />";
    mapnik_map_t * map;
    map = mapnik_map(256,256);
    mapnik_register_datasources(MAPNIK_PLUGINDIR, NULL);
    REQUIRE_FALSE(mapnik_map_load_string(map, minimal_map_string));
    mapnik_map_free(map);
}

TEST_CASE( "map/render", "should render png" ) {
      mapnik_map_t * map;
      map = mapnik_map(256,256);
      mapnik_register_datasources(MAPNIK_PLUGINDIR, NULL);
      REQUIRE_FALSE(mapnik_map_load(map,"sample/stylesheet.xml"));
      mapnik_map_zoom_all(map);
      mapnik_map_render_to_file(map,"/tmp/mapnik-c-api-test-map1.png");
      printf("\x1b[1;32m ✓ (%s)\x1b[0m\n", "rendered to /tmp/mapnik-c-api-test-map1.png");
      mapnik_map_free(map);
}

TEST_CASE( "map/render_to_mem", "should render png in memory" ) {
      mapnik_map_t * map;
      map = mapnik_map(1024,1024);
      mapnik_register_datasources(MAPNIK_PLUGINDIR, NULL);
      REQUIRE_FALSE(mapnik_map_load(map,"sample/stylesheet.xml"));
      mapnik_map_zoom_to_box(map, mapnik_bbox(0, 0, 5000000, 5000000));
      mapnik_image_t * i = mapnik_map_render_to_image(map);
      mapnik_image_blob_t * b = mapnik_image_to_png_blob(i);

      std::ofstream o("/tmp/mapnik-c-api-test-map2.png", std::ios_base::out | std::ios_base::binary);
      o.write(b->ptr, b->len);
      o.close();

      mapnik_image_blob_free(b);
      mapnik_image_free(i);
      mapnik_map_free(map);
}

TEST_CASE( "map/last_error", "should return errors" ) {
      mapnik_map_t * map;
      map = mapnik_map(1024,1024);
      const char * error;
      error = mapnik_map_last_error(map);
      REQUIRE( NULL==error );

      REQUIRE( -1==mapnik_map_load(map,"sample/doesnotexist.xml") );
      error = mapnik_map_last_error(map);
      REQUIRE( NULL!=strstr(error, "does not exist") );
      REQUIRE( NULL!=strstr(error, "sample/doesnotexist.xml") );

      REQUIRE_FALSE(mapnik_map_load(map,"sample/missing_attribute.xml"));
      // successful load resets last_error
      error = mapnik_map_last_error(map);
      REQUIRE( NULL==error );

      mapnik_map_zoom_to_box(map, mapnik_bbox(0, 0, 5000000, 5000000));

      // render to image should fail for missing attribute in style
      mapnik_image_t * i = mapnik_map_render_to_image(map);
      REQUIRE( NULL==i );
      error = mapnik_map_last_error(map);
      REQUIRE( NULL!=strstr(error, "no attribute 'doesnotexist' in") );

      // render to file should fail for missing attribute in style
      REQUIRE( -1==mapnik_map_render_to_file(map, "/tmp/mapnik-c-api-test-map3.png") );
      error = mapnik_map_last_error(map);
      REQUIRE( NULL!=strstr(error, "no attribute 'doesnotexist' in") );

      mapnik_map_free(map);
}

TEST_CASE( "map/null", "should not dereference a null map" ) {
      REQUIRE( mapnik_map_set_srs(NULL, "+proj=longlat +datum=WGS84 +no_defs") == -1 );
      REQUIRE( mapnik_map_copy(NULL) == NULL );
      REQUIRE( mapnik_map_render_to_image(NULL) == NULL );
      mapnik_map_set_buffer_size(NULL, 128);
}

TEST_CASE( "map/copy", "should copy everything the map was loaded with" ) {
      mapnik_map_t * map;
      map = mapnik_map(256,256);
      mapnik_register_datasources(MAPNIK_PLUGINDIR, NULL);
      REQUIRE_FALSE(mapnik_map_load(map,"sample/stylesheet.xml"));
      mapnik_map_zoom_all(map);
      mapnik_map_t * copy = mapnik_map_copy(map);
      REQUIRE( 0==strcmp(mapnik_map_get_srs(copy),mapnik_map_get_srs(map)) );
      mapnik_image_t * i1 = mapnik_map_render_to_image(map);
      mapnik_image_t * i2 = mapnik_map_render_to_image(copy);
      mapnik_image_blob_t * b1 = mapnik_image_to_png_blob(i1);
      mapnik_image_blob_t * b2 = mapnik_image_to_png_blob(i2);
      REQUIRE( b1->len == b2->len );
      REQUIRE( 0==memcmp(b1->ptr,b2->ptr,b1->len) );
      mapnik_image_blob_free(b1);
      mapnik_image_blob_free(b2);
      mapnik_image_free(i1);
      mapnik_image_free(i2);
      mapnik_map_free(copy);
      mapnik_map_free(map);
}

TEST_CASE( "projection/forward", "should project lon/lat degrees into the map srs" ) {
      mapnik_map_t * map;
      map = mapnik_map(256,256);
      mapnik_map_set_srs(map,"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs");
      mapnik_projection_t * proj = mapnik_map_projection(map);
      mapnik_coord_t c = {-122.4194, 37.7749};
      c = mapnik_projection_forward(proj, c);
      REQUIRE( c.x == Approx(-13627665.27).epsilon(1e-9) );
      REQUIRE( c.y == Approx(4547675.35).epsilon(1e-9) );
      mapnik_projection_free(proj);
      mapnik_map_free(map);
}
