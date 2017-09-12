from gppylib.gparray import get_gparray_from_config


class MirrorMatchingCheck:

    def run_check(self, db_connection, logger):
        logger.info('-----------------------------------')
        logger.info('Checking mirroring_matching')

        is_config_mirror_enabled = get_gparray_from_config().hasMirrors

        # This query returns the mirroring status of all segments
        mirroring_query = """SELECT gp_segment_id, mirror_existence_state FROM gp_dist_random('gp_persistent_relation_node') GROUP BY 1,2"""
        segment_mirroring_result = db_connection.query(mirroring_query).getresult()

        mismatching_segments = []
        for (seg_id, mirror_state) in segment_mirroring_result:
            is_segment_mirrored = mirror_state > 1
            if mirror_state == 0:
                continue  # 0 is considered a match in either situation
            if is_segment_mirrored != is_config_mirror_enabled:
                mismatching_segments.append((seg_id, mirror_state))

        if mismatching_segments:
            logger.info('[FAIL] Mirroring mismatch detected')
            logger.info("The GP configuration reports mirror enabling is: %s" % is_config_mirror_enabled)
            logger.error("The following segments are mismatched in PT:")
            logger.error("")
            logger.error("Segment ID:\tmirror_existence_state:")
            for (seg_id, mirror_existence_state) in mismatching_segments:
                label = "Enabled" if mirror_existence_state > 1 else "Disabled"
                logger.error("%i\t\t%i (%s)" % (seg_id, mirror_existence_state, label))
        else:
            logger.info('[OK] %s' % "mirroring_matching")

        return mismatching_segments
