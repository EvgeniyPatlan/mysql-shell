#@<OUT> util help
NAME
      util - Global object that groups miscellaneous tools like upgrade checker
             and JSON import.

DESCRIPTION
      Global object that groups miscellaneous tools like upgrade checker and
      JSON import.

FUNCTIONS
      check_for_server_upgrade([connectionData][, options])
            Performs series of tests on specified MySQL server to check if the
            upgrade process will succeed.

?{__with_oci==1}
      configure_oci([profile])
            Wizard to create a valid configuration for the OCI SDK.

?{}
      help([member])
            Provides help about this object and it's members

      import_json(file[, options])
            Import JSON documents from file to collection or table in MySQL
            Server using X Protocol session.

      import_table(filename[, options])
            Import table dump stored in filename to target table using LOAD
            DATA LOCAL INFILE calls in parallel connections.

#@<OUT> util check_for_server_upgrade help
NAME
      check_for_server_upgrade - Performs series of tests on specified MySQL
                                 server to check if the upgrade process will
                                 succeed.

SYNTAX
      util.check_for_server_upgrade([connectionData][, options])

WHERE
      connectionData: The connection data to server to be checked
      options: Dictionary of options to modify tool behaviour.

DESCRIPTION
      If no connectionData is specified tool will try to establish connection
      using data from current session.

      Tool behaviour can be modified with following options:

      - configPath - full path to MySQL server configuration file.
      - outputFormat - value can be either TEXT (default) or JSON.
      - targetVersion - version to which upgrade will be checked
        (default=<<<__mysh_version>>>)
      - password - password for connection.

      The connection data may be specified in the following formats:

      - A URI string
      - A dictionary with the connection options

      A basic URI string has the following format:

      [scheme://][user[:password]@]<host[:port]|socket>[/schema][?option=value&option=value...]

      Connection Options

      The following options are valid for use either in a URI or in a
      dictionary:

      - ssl-mode: the SSL mode to be used in the connection.
      - ssl-ca: the path to the X509 certificate authority in PEM format.
      - ssl-capath: the path to the directory that contains the X509
        certificates authorities in PEM format.
      - ssl-cert: The path to the X509 certificate in PEM format.
      - ssl-key: The path to the X509 key in PEM format.
      - ssl-crl: The path to file that contains certificate revocation lists.
      - ssl-crlpath: The path of directory that contains certificate revocation
        list files.
      - ssl-cipher: SSL Cipher to use.
      - tls-version: List of protocols permitted for secure connections.
      - tls-ciphers: List of TLS v1.3 ciphers to use.
      - auth-method: Authentication method
      - get-server-public-key: Request public key from the server required for
        RSA key pair-based password exchange. Use when connecting to MySQL 8.0
        servers with classic MySQL sessions with SSL mode DISABLED.
      - server-public-key-path: The path name to a file containing a
        client-side copy of the public key required by the server for RSA key
        pair-based password exchange. Use when connecting to MySQL 8.0 servers
        with classic MySQL sessions with SSL mode DISABLED.
      - connect-timeout: The connection timeout in milliseconds. If not
        provided a default timeout of 10 seconds will be used. Specifying a
        value of 0 disables the connection timeout.
      - compression: Enable/disable compression in client/server protocol,
        valid values: "true", "false", "1", and "0".
      - connection-attributes: List of connection attributes to be registered
        at the PERFORMANCE_SCHEMA connection attributes tables.
      - local-infile: Enable/disable LOAD DATA LOCAL INFILE.
      - net-buffer-length: The buffer size for TCP/IP and socket communication.

      When these options are defined in a URI, their values must be URL
      encoded.

      The following options are also valid when a dictionary is used:

      Base Connection Options

      - scheme: the protocol to be used on the connection.
      - user: the MySQL user name to be used on the connection.
      - dbUser: alias for user.
      - password: the password to be used on the connection.
      - dbPassword: same as password.
      - host: the hostname or IP address to be used on the connection.
      - port: the port to be used in a TCP connection.
      - socket: the socket file name to be used on a connection through unix
        sockets.
      - schema: the schema to be selected once the connection is done.

      ATTENTION: The dbUser and dbPassword options are will be removed in a
                 future release.

      The connection options are case insensitive and can only be defined once.

      If an option is defined more than once, an error will be generated.

      For additional information on connection data use \? connection.

#@<OUT> util import_json help
NAME
      import_json - Import JSON documents from file to collection or table in
                    MySQL Server using X Protocol session.

SYNTAX
      util.import_json(file[, options])

WHERE
      file: Path to JSON documents file
      options: Dictionary with import options

DESCRIPTION
      This function reads standard JSON documents from a file, however, it also
      supports converting BSON Data Types represented using the MongoDB
      Extended Json (strict mode) into MySQL values.

      The options dictionary supports the following options:

      - schema: string - name of target schema.
      - collection: string - name of collection where the data will be
        imported.
      - table: string - name of table where the data will be imported.
      - tableColumn: string (default: "doc") - name of column in target table
        where the imported JSON documents will be stored.
      - convertBsonTypes: bool (default: false) - enables the BSON data type
        conversion.
      - convertBsonOid: bool (default: the value of convertBsonTypes) - enables
        conversion of the BSON ObjectId values.
      - extractOidTime: string (default: empty) - creates a new field based on
        the ObjectID timestamp. Only valid if convertBsonOid is enabled.

      The following options are valid only when convertBsonTypes is enabled.
      They are all boolean flags. ignoreRegexOptions is enabled by default,
      rest are disabled by default.

      - ignoreDate: disables conversion of BSON Date values
      - ignoreTimestamp: disables conversion of BSON Timestamp values
      - ignoreRegex: disables conversion of BSON Regex values.
      - ignoreBinary: disables conversion of BSON BinData values.
      - decimalAsDouble: causes BSON Decimal values to be imported as double
        values.
      - ignoreRegexOptions: causes regex options to be ignored when processing
        a Regex BSON value. This option is only valid if ignoreRegex is
        disabled.

      If the schema is not provided, an active schema on the global session, if
      set, will be used.

      The collection and the table options cannot be combined. If they are not
      provided, the basename of the file without extension will be used as
      target collection name.

      If the target collection or table does not exist, they are created,
      otherwise the data is inserted into the existing collection or table.

      The tableColumn implies the use of the table option and cannot be
      combined with the collection option.

      BSON Data Type Processing.

      If only convertBsonOid is enabled, no conversion will be done on the rest
      of the BSON Data Types.

      To use extractOidTime, it should be set to a name which will be used to
      insert an additional field into the main document. The value of the new
      field will be the timestamp obtained from the ObjectID value. Note that
      this will be done only for an ObjectID value associated to the '_id'
      field of the main document.

      NumberLong and NumberInt values will be converted to integer values.

      NumberDecimal values are imported as strings, unless decimalAsDouble is
      enabled.

      Regex values will be converted to strings containing the regular
      expression. The regular expression options are ignored unless
      ignoreRegexOptions is disabled. When ignoreRegexOptions is disabled the
      regular expression will be converted to the form: /<regex>/<options>.

EXCEPTIONS
      Throws ArgumentError when:

      - Option name is invalid
      - Required options are not set and cannot be deduced
      - Shell is not connected to MySQL Server using X Protocol
      - Schema is not provided and there is no active schema on the global
        session
      - Both collection and table are specified

      Throws LogicError when:

      - Path to JSON document does not exists or is not a file

      Throws RuntimeError when:

      - The schema does not exists
      - MySQL Server returns an error

      Throws InvalidJson when:

      - JSON document is ill-formed

#@<OUT> util configure_oci help
NAME
      configure_oci - Wizard to create a valid configuration for the OCI SDK.

SYNTAX
      util.configure_oci([profile])

WHERE
      profile: Parameter to specify the name profile to be configured.

DESCRIPTION
      If the profile name is not specified 'DEFAULT' will be used.

      To properly create OCI configuration to use the OCI SDK the following
      information will be required:

      - User OCID
      - Tenancy OCID
      - Tenancy region
      - A valid API key

      For details about where to find the user and tenancy details go to
      https://docs.us-phoenix-1.oraclecloud.com/Content/API/Concepts/apisigningkey.htm#Other

#@<OUT> oci help
The MySQL Shell offers support for the Oracle Cloud Infrastructure (OCI).

After starting the MySQL Shell with the --oci option an interactive wizard will
help to create the correct OCI configuration file, load the OCI Python SDK and
switch the shell to Python mode.

The MySQL Shell can then be used to access the OCI APIs and manage the OCI
account.

The following Python objects are automatically initialized.

- config an OCI configuration object with data loaded from ~/.oci/config
- identity an OCI identity client object, offering APIs for managing users,
  groups, compartments and policies.
- compute an OCI compute client object, offering APIs for Networking Service,
  Compute Service, and Block Volume Service.

For more information about the OCI Python SDK please read the documentation at
https://oracle-cloud-infrastructure-python-sdk.readthedocs.io/en/latest/

EXAMPLES
identity.get_user(config['user']).data
      Fetches information about the OCI user account specified in the config
      object.

identity.list_compartments(config['tenancy']).data
      Fetches the list of top level compartments available in the tenancy that
      was specified in the config object.

compartment = identity.list_compartments(config['tenancy']).data[0]
images = compute.list_images(compartment.id).data
for image in images:
  print(image.display_name)
      Assignes the first compartment of the tenancy to the compartment
      variable, featches the available OS images for the compartment and prints
      a list of their names.

#@<OUT> util import_table help
NAME
      import_table - Import table dump stored in filename to target table using
                     LOAD DATA LOCAL INFILE calls in parallel connections.

SYNTAX
      util.import_table(filename[, options])

WHERE
      filename: Path to file with user data
      options: Dictionary with import options

DESCRIPTION
      Scheme part of filename contains infomation about the transport backend.
      Supported transport backends are: file://, http://, https://, oci+os://.
      If scheme part of filename is omitted, then file:// transport backend
      will be chosen.

      Supported filename formats:

      - [file://]/path/to/file - Read import data from local file
      - http[s]://host.domain[:port]/path/to/file - Read import data from file
        provided in URL
      - oci+os://namespace/region/bucket/object - Read import data from object
        stored in OCI (Oracle Cloud Infrastructure) Object Storage. Variables
        needed to sign requests will be obtained from profile configured in OCI
        configuration file. Profile name and configuration file path are
        specified in oci.profile and oci.configFile shell options. ociProfile
        and ociConfigFile options will override, respectively, oci.profile and
        oci.configFile shell options.

      Options dictionary:

      - schema: string (default: current shell active schema) - Name of target
        schema
      - table: string (default: filename without extension) - Name of target
        table
      - columns: string array (default: empty array) - This option takes a
        array of column names as its value. The order of the column names
        indicates how to match data file columns with table columns.
      - fieldsTerminatedBy: string (default: "\t"), fieldsEnclosedBy: char
        (default: ''), fieldsEscapedBy: char (default: '\') - These options
        have the same meaning as the corresponding clauses for LOAD DATA
        INFILE. For more information use \? LOAD DATA, (a session is required).
      - fieldsOptionallyEnclosed: bool (default: false) - Set to true if the
        input values are not necessarily enclosed within quotation marks
        specified by fieldsEnclosedBy option. Set to false if all fields are
        quoted by character specified by fieldsEnclosedBy option.
      - linesTerminatedBy: string (default: "\n") - This option has the same
        meaning as the corresponding clause for LOAD DATA INFILE. For example,
        to import Windows files that have lines terminated with carriage
        return/linefeed pairs, use --lines-terminated-by="\r\n". (You might
        have to double the backslashes, depending on the escaping conventions
        of your command interpreter.) See Section 13.2.7, "LOAD DATA INFILE
        Syntax".
      - replaceDuplicates: bool (default: false) - If true, input rows that
        have the same value for a primary key or unique index as an existing
        row will be replaced, otherwise input rows will be skipped.
      - threads: int (default: 8) - Use N threads to sent file chunks to the
        server.
      - bytesPerChunk: string (minimum: "131072", default: "50M") - Send
        bytesPerChunk (+ bytes to end of the row) in single LOAD DATA call.
        Unit suffixes, k - for Kilobytes (n * 1'000 bytes), M - for Megabytes
        (n * 1'000'000 bytes), G - for Gigabytes (n * 1'000'000'000 bytes),
        bytesPerChunk="2k" - ~2 kilobyte data chunk will send to the MySQL
        Server.
      - maxRate: string (default: "0") - Limit data send throughput to maxRate
        in bytes per second per thread. maxRate="0" - no limit. Unit suffixes,
        k - for Kilobytes (n * 1'000 bytes), M - for Megabytes (n * 1'000'000
        bytes), G - for Gigabytes (n * 1'000'000'000 bytes), maxRate="2k" -
        limit to 2 kilobytes per second.
      - showProgress: bool (default: true if stdout is a tty, false otherwise)
        - Enable or disable import progress information.
      - skipRows: int (default: 0) - Skip first n rows of the data in the file.
        You can use this option to skip an initial header line containing
        column names.
      - dialect: enum (default: "default") - Setup fields and lines options
        that matches specific data file format. Can be used as base dialect and
        customized with fieldsTerminatedBy, fieldsEnclosedBy,
        fieldsOptionallyEnclosed, fieldsEscapedBy and linesTerminatedBy
        options. Must be one of the following values: csv, tsv, json or
        csv-unix.
      - ociConfigFile: string (default: not set) - Override oci.configFile
        shell option. Available only if oci+os:// transport protocol is in use.
      - ociProfile: string (default: not set) - Override oci.profile shell
        option. Available only if oci+os:// transport protocol is in use.

      dialect predefines following set of options fieldsTerminatedBy (FT),
      fieldsEnclosedBy (FE), fieldsOptionallyEnclosed (FOE), fieldsEscapedBy
      (FESC) and linesTerminatedBy (LT) in following manner:

      - default: no quoting, tab-separated, lf line endings. (LT=<LF>,
        FESC='\', FT=<TAB>, FE=<empty>, FOE=false)
      - csv: optionally quoted, comma-separated, crlf line endings.
        (LT=<CR><LF>, FESC='\', FT=",", FE='"', FOE=true)
      - tsv: optionally quoted, tab-separated, crlf line endings. (LT=<CR><LF>,
        FESC='\', FT=<TAB>, FE='"', FOE=true)
      - json: one JSON document per line. (LT=<LF>, FESC=<empty>, FT=<LF>,
        FE=<empty>, FOE=false)
      - csv-unix: fully quoted, comma-separated, lf line endings. (LT=<LF>,
        FESC='\', FT=",", FE='"', FOE=false)

      Example input data for dialects:

      - default:
      1<TAB>20.1000<TAB>foo said: "Where is my bar?"<LF>
      2<TAB>-12.5000<TAB>baz said: "Where is my \<TAB> char?"<LF>
      - csv:
      1,20.1000,"foo said: \"Where is my bar?\""<CR><LF>
      2,-12.5000,"baz said: \"Where is my <TAB> char?\""<CR><LF>
      - tsv:
      1<TAB>20.1000<TAB>"foo said: \"Where is my bar?\""<CR><LF>
      2<TAB>-12.5000<TAB>"baz said: \"Where is my <TAB> char?\""<CR><LF>
      - json:
      {"id_int": 1, "value_float": 20.1000, "text_text": "foo said: \"Where is
      my bar?\""}<LF>
      {"id_int": 2, "value_float": -12.5000, "text_text": "baz said: \"Where is
      my \u000b char?\""}<LF>
      - csv-unix:
      "1","20.1000","foo said: \"Where is my bar?\""<LF>
      "2","-12.5000","baz said: \"Where is my <TAB> char?\""<LF>

      If the schema is not provided, an active schema on the global session, if
      set, will be used.

      If the input values are not necessarily enclosed within fieldsEnclosedBy,
      set fieldsOptionallyEnclosed to true.

      If you specify one separator that is the same as or a prefix of another,
      LOAD DATA INFILE cannot interpret the input properly.

      Connection options set in the global session, such as compression,
      ssl-mode, etc. are used in parallel connections.

      Each parallel connection sets the following session variables:

      - SET unique_checks = 0
      - SET foreign_key_checks = 0
      - SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED

