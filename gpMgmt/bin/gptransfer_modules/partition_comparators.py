import re

PARTITION_METADATA_PATTERN = re.compile('.*CONST (:.*) \[')
KEY_WITH_METADATA_43_TO_5 = ["parrangestart", "parrangeend", "parrangeevery", "parlistvalues"]
KEY_WITH_METADATA = ["version", "parrangestartincl"] + KEY_WITH_METADATA_43_TO_5


class PartitionComparator(object):
    def _parse_value(self, partition_info_dict, column_list):
        """
        input like:
        (({CONST :consttype 1042 :constlen -1 :constbyval false :constisnull false :constvalue 5 [ 0 0 0 5 77 ]}
          {CONST :consttype 23 :constlen 4 :constbyval true :constisnull false :constvalue 4 [ 1 0 0 0 0 0 0 0 ]}))

        break into chunks, sort the chunks by alpha,
        :returns a list of all text chunks split by a space
        """
        result = dict(partition_info_dict)
        for column_key in column_list:
            value = result[column_key]
            # removing enclosing parenthesis and curly braces '(({' and '}))'
            value = value[3:-3]
            chunks = value.split("} {")
            alpha_chunks = sorted(chunks)
            result[column_key] = []
            for chunk in alpha_chunks:
                result[column_key].extend(chunk.split(" "))
        return result


class Version4toXPartitionComparator(PartitionComparator):

    def _parse_value(self, partition_info_dict):
        return super(Version4toXPartitionComparator, self)._parse_value(partition_info_dict,
                                                                        KEY_WITH_METADATA_43_TO_5)

    def _remove_key_and_value(self, list_info, key_to_remove):
        while key_to_remove in list_info:
            index = list_info.index(key_to_remove)
            # delete also removes the corresponding value
            del list_info[index:index+2]

    def is_same(self, source_partition_info, dest_partition_info):
        """
        TODO: compare attributes not used for list or range, e.g., parisdefault
        """
        src_dict = self._parse_value(source_partition_info)
        dest_dict = self._parse_value(dest_partition_info)

        for key in KEY_WITH_METADATA_43_TO_5:
            version_info = dest_dict[key]
            # greenplum 5 added a 'consttypmod' attribute.
            # remove it so as to compare all other attributes
            self._remove_key_and_value(version_info, ':consttypmod')
            if src_dict[key] != dest_dict[key]:
                return False
        return True


class SameVersionPartitionComparator(PartitionComparator):

    ALLOW_UNORDERED_COLUMNS_LIST = ['parlistvalues']

    def _parse_value(self, partition_info_dict):
        return super(SameVersionPartitionComparator, self)._parse_value(partition_info_dict,
                                                                        self.ALLOW_UNORDERED_COLUMNS_LIST)

    def is_same(self, source_partition_info, dest_partition_info):
        src_dict = self._parse_value(source_partition_info)
        dest_dict = self._parse_value(dest_partition_info)

        for key in KEY_WITH_METADATA:
            if src_dict[key] != dest_dict[key]:
                return False
        return True


class PartitionComparatorFactory:
    def get(self, source, dest):
        source_ver = source['version']
        dest_ver = dest['version']
        if source_ver == dest_ver:
            return SameVersionPartitionComparator()
        elif source_ver == '4.3' and dest_ver != '4.3':
            return Version4toXPartitionComparator()

        raise Exception("No comparator defined for source "
                        "and dest versions\n: %s \n\n %s" % (source, dest))
